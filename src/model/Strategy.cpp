#include "model/Strategy.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <thread>

namespace yatze::model::strategy {

// ─────────────────────────────────────────────────────────────────────────────
// The combinatorics, built once: every multiset of dice of size 0..5 (a KEEP;
// the size-5 ones are the 252 ROLLS), what rolling the rest of the dice from a
// keep can give and how likely, and which keeps each roll contains.
// ─────────────────────────────────────────────────────────────────────────────
namespace {

using Counts = std::array<std::uint8_t, 7>;   // [1..6] used

int key_of(const Counts& c) {
    int k = 0;
    for (int f = 6; f >= 1; --f) k = k * 6 + c[f];
    return k;   // < 6^6
}

struct Combo {
    std::vector<Counts> keeps;           // all multisets of size 0..5
    std::vector<int>    keep_size;
    std::vector<int>    rolls;           // keep index of each size-5 multiset (252)
    std::vector<int>    roll_of_keep;    // keep index -> roll index (or -1)
    std::vector<float>  roll_p;          // P(rolling this from nothing)
    std::vector<int>    tr_start, tr_roll;   // keep -> (roll, p) list
    std::vector<float>  tr_p;
    std::vector<int>    sub_start, sub_keep; // roll -> keeps it contains
    std::vector<std::array<int, kCategories>> base;   // roll x box -> points
    std::vector<bool>   yatze;           // roll is five alike
    std::vector<int>    face_of;         // its face, if so
    std::vector<int>    keep_index;      // key -> keep index

    Combo() {
        keep_index.assign(46656, -1);
        // Enumerate multisets of size n by counts.
        std::vector<std::vector<Counts>> by_size(6);
        Counts c{};
        auto rec = [&](auto&& self, int face, int left) -> void {
            if (face == 7) { if (left >= 0) by_size[5 - left].push_back(c); return; }
            for (int k = 0; k <= left; ++k) { c[face] = std::uint8_t(k); self(self, face + 1, left - k); }
            c[face] = 0;
        };
        rec(rec, 1, 5);
        for (int n = 0; n <= 5; ++n)
            for (const auto& m : by_size[n]) {
                keep_index[key_of(m)] = int(keeps.size());
                keeps.push_back(m);
                keep_size.push_back(n);
            }
        // Probability of a multiset of size n rolled from n dice.
        auto prob = [](const Counts& m, int n) {
            double num = std::tgamma(n + 1.0);
            for (int f = 1; f <= 6; ++f) num /= std::tgamma(m[f] + 1.0);
            return float(num / std::pow(6.0, n));
        };
        roll_of_keep.assign(keeps.size(), -1);
        for (int k = 0; k < int(keeps.size()); ++k)
            if (keep_size[k] == 5) {
                roll_of_keep[k] = int(rolls.size());
                rolls.push_back(k);
                roll_p.push_back(prob(keeps[k], 5));
            }
        // keep -> rolls: add every outcome of the 5 - size dice rolled.
        for (int k = 0; k < int(keeps.size()); ++k) {
            tr_start.push_back(int(tr_roll.size()));
            const int n = 5 - keep_size[k];
            for (const auto& o : by_size[n]) {
                Counts sum{};
                for (int f = 1; f <= 6; ++f) sum[f] = std::uint8_t(keeps[k][f] + o[f]);
                tr_roll.push_back(roll_of_keep[keep_index[key_of(sum)]]);
                tr_p.push_back(prob(o, n));
            }
        }
        tr_start.push_back(int(tr_roll.size()));
        // roll -> every sub-multiset (the keeps you could choose from it).
        for (int r = 0; r < int(rolls.size()); ++r) {
            sub_start.push_back(int(sub_keep.size()));
            const Counts& m = keeps[rolls[r]];
            Counts s{};
            auto sub = [&](auto&& self, int face) -> void {
                if (face == 7) { sub_keep.push_back(keep_index[key_of(s)]); return; }
                for (int k = 0; k <= m[face]; ++k) { s[face] = std::uint8_t(k); self(self, face + 1); }
                s[face] = 0;
            };
            sub(sub, 1);
        }
        sub_start.push_back(int(sub_keep.size()));
        // Points per roll per box, by the game's own rules.
        for (int r = 0; r < int(rolls.size()); ++r) {
            const Dice d = dice_of(r);
            std::array<int, kCategories> b{};
            for (int i = 0; i < kCategories; ++i) b[i] = base_score(category(i), d);
            base.push_back(b);
            yatze.push_back(is_yatze(d));
            face_of.push_back(d[0]);
        }
    }

    Dice dice_of(int r) const {
        Dice d{};
        int i = 0;
        const Counts& m = keeps[rolls[r]];
        for (int f = 1; f <= 6; ++f) for (int k = 0; k < m[f]; ++k) d[i++] = f;
        return d;
    }
    int roll_index(const Dice& d) const {
        Counts m{};
        for (int f : d) ++m[f];
        return roll_of_keep[keep_index[key_of(m)]];
    }
};

const Combo& combo() {
    static const Combo c;   // thread-safe init
    return c;
}

// The upper total the filled upper boxes could possibly have made.
int upper_max(std::uint16_t mask) {
    int m = 0;
    for (int i = 0; i < kUpper; ++i) if (mask & (1u << i)) m += 5 * (i + 1);
    return std::min(m, kBonusAt);
}

// Every legal move for roll r in state s: calls f(cat, points, gain, next).
// The Joker logic mirrors score_options exactly (cross-checked in the selftest).
template <class F>
void for_each_move(const State& s, int r, F&& f) {
    const Combo& C = combo();
    const auto& b = C.base[r];
    auto open = [&](int i) { return !(s.mask & (1u << i)); };
    auto take = [&](int i, int pts, int extra) {
        State n = s;
        n.mask = std::uint16_t(s.mask | (1u << i));
        int gain = pts + extra;
        if (i < kUpper) {
            const int u = std::min(kBonusAt, s.upper + pts);
            if (s.upper < kBonusAt && s.upper + pts >= kBonusAt) gain += kUpperBonus;
            n.upper = u;
        }
        if (i == index(Category::Yatze) && pts == 50) n.y50 = true;
        f(category(i), pts, gain, n);
    };
    const bool joker = C.yatze[r] && !open(index(Category::Yatze));
    if (!joker) {
        for (int i = 0; i < kCategories; ++i) if (open(i)) take(i, b[i], 0);
        return;
    }
    const int bonus = s.y50 ? kYatzeBonus : 0;
    const int fbox  = C.face_of[r] - 1;
    if (open(fbox)) { take(fbox, b[fbox], bonus); return; }
    bool any_lower = false;
    for (int i = kUpper; i < kCategories; ++i) {
        if (!open(i)) continue;
        any_lower = true;
        int pts = b[i];
        if (category(i) == Category::FullHouse)     pts = 25;
        if (category(i) == Category::SmallStraight) pts = 30;
        if (category(i) == Category::LargeStraight) pts = 40;
        take(i, pts, bonus);
    }
    if (any_lower) return;
    for (int i = 0; i < kUpper; ++i) if (open(i)) take(i, 0, bonus);
}

// The turn, solved, for one state. s0[r]: best value of scoring roll r now.
// e[l][k]: expected value of keeping k with l+1 rolls of the turn still to
// come after this keep is rolled... precisely: e0 = E[s0 | keep], v1 = best
// keep under e0, e1 = E[v1 | keep], v2 = best keep under e1.
struct Turn {
    std::array<float, 252> s0, v1, v2;
    std::vector<float>     e0, e1;
    Turn() : e0(462), e1(462) {}

    void expect(const std::array<float, 252>& v, std::vector<float>& e) const {
        const Combo& C = combo();
        for (int k = 0; k < 462; ++k) {
            float acc = 0;
            for (int t = C.tr_start[k]; t < C.tr_start[k + 1]; ++t) acc += C.tr_p[t] * v[C.tr_roll[t]];
            e[k] = acc;
        }
    }
    void best_keep(const std::vector<float>& e, std::array<float, 252>& v) const {
        const Combo& C = combo();
        for (int r = 0; r < 252; ++r) {
            float m = -1;
            for (int t = C.sub_start[r]; t < C.sub_start[r + 1]; ++t) m = std::max(m, e[C.sub_keep[t]]);
            v[r] = m;
        }
    }
    // Fill s0 from the future values, then the rest. Returns E[turn from scratch].
    template <class Future>
    float solve(const State& s, Future&& future, int depth = 2) {
        for (int r = 0; r < 252; ++r) {
            float best = -1;
            for_each_move(s, r, [&](Category, int, int gain, const State& n) {
                best = std::max(best, float(gain) + future(n));
            });
            s0[r] = best;
        }
        expect(s0, e0);
        if (depth < 1) return 0;
        best_keep(e0, v1);
        expect(v1, e1);
        if (depth < 2) return 0;
        best_keep(e1, v2);
        const Combo& C = combo();
        float acc = 0;
        for (int r = 0; r < 252; ++r) acc += C.roll_p[r] * v2[r];
        return acc;
    }
};

constexpr std::uint32_t kMagic   = 0x59545a31;   // "YTZ1"
constexpr std::uint32_t kVersion = 1;            // bump when the rules or the state change

}  // namespace

State state_of(const Card& c) {
    State s;
    for (int i = 0; i < kCategories; ++i) if (!c.open(category(i))) s.mask |= std::uint16_t(1u << i);
    s.upper = std::min(kBonusAt, c.upper_sum());
    s.y50   = c.box[index(Category::Yatze)] == 50;
    return s;
}

std::vector<Move> moves(const State& s, const Dice& d) {
    std::vector<Move> out;
    if (!rolled(d)) return out;
    for_each_move(s, combo().roll_index(d), [&](Category c, int pts, int gain, const State& n) {
        out.push_back({c, pts, gain, n});
    });
    return out;
}

// ── the table ───────────────────────────────────────────────────────────────
bool Table::build(int threads, const std::atomic<bool>* cancel, std::atomic<float>* progress) {
    (void)combo();
    std::vector<float> f(kStates, 0.0f);
    if (threads <= 0) threads = std::max(1u, std::thread::hardware_concurrency());

    // Levels by number of boxes filled, from 12 down to 0 (13 = full card = 0).
    std::vector<std::vector<int>> level(kCategories + 1);
    for (int mask = 0; mask < (1 << kCategories); ++mask) {
        const int pc = std::popcount(unsigned(mask));
        if (pc == kCategories) continue;
        const bool ybox = mask & (1 << index(Category::Yatze));
        for (int up = 0; up <= upper_max(std::uint16_t(mask)); ++up)
            for (int y = 0; y <= (ybox ? 1 : 0); ++y)
                level[pc].push_back(slot(State{std::uint16_t(mask), up, bool(y)}));
    }
    std::size_t total = 0, done = 0;
    for (const auto& l : level) total += l.size();

    for (int pc = kCategories - 1; pc >= 0; --pc) {
        const auto& todo = level[pc];
        std::atomic<std::size_t> next{0};
        auto work = [&] {
            Turn t;
            const auto future = [&](const State& n) { return f[std::size_t(slot(n))]; };
            for (;;) {
                if (cancel && cancel->load()) return;
                const std::size_t i = next.fetch_add(1);
                if (i >= todo.size()) return;
                const int sl = todo[i];
                State s;
                s.y50   = sl & 1;
                s.upper = (sl >> 1) & 63;
                s.mask  = std::uint16_t(sl >> 7);
                f[std::size_t(sl)] = t.solve(s, future);
            }
        };
        std::vector<std::thread> pool;
        for (int i = 1; i < threads; ++i) pool.emplace_back(work);
        work();
        for (auto& th : pool) th.join();
        if (cancel && cancel->load()) return false;
        done += todo.size();
        if (progress) progress->store(float(done) / float(total));
    }
    m_f = std::move(f);
    return true;
}

bool Table::save(const std::string& path) const {
    if (!ready()) return false;
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    const std::uint32_t head[3] = {kMagic, kVersion, std::uint32_t(kStates)};
    out.write(reinterpret_cast<const char*>(head), sizeof head);
    out.write(reinterpret_cast<const char*>(m_f.data()), std::streamsize(m_f.size() * sizeof(float)));
    return bool(out);
}

bool Table::load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::uint32_t head[3]{};
    in.read(reinterpret_cast<char*>(head), sizeof head);
    if (!in || head[0] != kMagic || head[1] != kVersion || head[2] != std::uint32_t(kStates)) return false;
    std::vector<float> f(kStates);
    in.read(reinterpret_cast<char*>(f.data()), std::streamsize(f.size() * sizeof(float)));
    if (!in) return false;
    m_f = std::move(f);
    return true;
}

// ── advice ──────────────────────────────────────────────────────────────────
Advice advise(const Table& t, const Card& card, const Dice& dice, int rolls_left) {
    Advice a;
    if (!t.ready() || !rolled(dice) || card.full()) return a;
    const Combo& C = combo();
    const State s = state_of(card);
    const double base = card.total();
    const auto future = [&](const State& n) { return t.future(n); };

    for (const auto& m : moves(s, dice))
        a.boxes.push_back({m.c, m.points, m.gain, base + m.gain + future(m.next)});
    std::stable_sort(a.boxes.begin(), a.boxes.end(),
                     [](const BoxAdvice& x, const BoxAdvice& y) { return x.expect > y.expect; });
    if (rolls_left <= 0) return a;

    Turn turn;
    turn.solve(s, future, rolls_left >= 2 ? 1 : 0);
    const std::vector<float>& e = rolls_left >= 2 ? turn.e1 : turn.e0;

    const int r = C.roll_index(dice);
    for (int x = C.sub_start[r]; x < C.sub_start[r + 1]; ++x) {
        const int k = C.sub_keep[x];
        if (C.keep_size[k] == 5) continue;            // keeping all = take a box now
        HoldAdvice h;
        h.expect = base + e[k];
        h.count  = C.keep_size[k];
        // Faces to keep -> dice positions, left to right.
        Counts need = C.keeps[k];
        int n = 0;
        for (int f = 1; f <= 6; ++f) for (int q = 0; q < need[f]; ++q) h.kept[n++] = f;
        for (int i = 0; i < 5; ++i)
            if (need[dice[i]] > 0) { h.hold[i] = true; --need[dice[i]]; }
        a.holds.push_back(h);
    }
    std::stable_sort(a.holds.begin(), a.holds.end(),
                     [](const HoldAdvice& x, const HoldAdvice& y) { return x.expect > y.expect; });
    a.roll_on = !a.holds.empty() && !a.boxes.empty() && a.holds.front().expect > a.boxes.front().expect + 1e-4;
    return a;
}

}  // namespace yatze::model::strategy
