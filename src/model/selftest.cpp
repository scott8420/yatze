// yatze_selftest -- the headless model harness. No display.
//   ./build/yatze_selftest   (exit 0 = all pass)
//
// Rules: every category on fixtures, straights at their edges, the upper bonus
// at 63, every Joker branch and the +100. Game: turn flow, refusals, the end.
// Undo: every verb through the one door (the gate), undo puts the picture back,
// and the FATE -- undo a roll, roll again, same dice.

#include "model/Drawers.hpp"
#include "model/Game.hpp"
#include "model/Rules.hpp"
#include "model/Shortcuts.hpp"
#include "model/Strategy.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace model = yatze::model;
using model::Category;
using model::Dice;

namespace {
int g_pass = 0, g_fail = 0;
void check(const std::string& name, bool ok, const std::string& detail = "") {
    if (ok) { ++g_pass; std::cout << "  PASS  " << name; }
    else    { ++g_fail; std::cout << "  FAIL  " << name; }
    if (!detail.empty()) std::cout << "  (" << detail << ")";
    std::cout << "\n";
}
std::string str(const Dice& d) {
    std::string s;
    for (int f : d) s += char('0' + f);
    return s;
}
int pts(Category c, const Dice& d) { return model::base_score(c, d); }

// A store + journal wired the way the Shell wires them.
struct Fix {
    model::GameStore   s;
    model::GameJournal j{s};
    Fix(int players = 1, std::uint64_t seed = 42) {
        s.set_door([this] { return j.open(); });
        std::vector<model::Player> ps;
        for (int i = 0; i < players; ++i) ps.push_back({"P" + std::to_string(i + 1)});
        model::verbs::new_game(s, j, ps, seed);
    }
    const model::GameState& g() const { return s.state(); }
};

// Force dice for a rules-through-the-game test: through the door, like a verb.
void set_dice(Fix& f, const Dice& d) {
    f.j.run("test dice", {model::GameStore::kGame}, [&] {
        auto g = f.s.state();
        g.dice = d;
        if (g.rolls_used == 0) g.rolls_used = 1;
        f.s.put(model::GameStore::kGame, g);
    });
}
}  // namespace

int main() {
    std::cout << "[rules: categories]\n";
    {
        const Dice d{3, 3, 3, 5, 2};
        check("upper: threes add up", pts(Category::Threes, d) == 9);
        check("upper: a face not there is 0", pts(Category::Sixes, d) == 0);
        check("3 of a kind: the sum of all", pts(Category::ThreeKind, d) == 16);
        check("4 of a kind: needs four", pts(Category::FourKind, d) == 0);
        check("4 of a kind: with four", pts(Category::FourKind, {4, 4, 4, 4, 1}) == 17);
        check("3 of a kind: five alike counts", pts(Category::ThreeKind, {6, 6, 6, 6, 6}) == 30);
        check("full house: 3 + 2", pts(Category::FullHouse, {2, 2, 5, 5, 5}) == 25);
        check("full house: not 4 + 1", pts(Category::FullHouse, {2, 5, 5, 5, 5}) == 0);
        check("full house: five alike is not one (no Joker)", pts(Category::FullHouse, {5, 5, 5, 5, 5}) == 0);
        check("small straight: 1234 + 6", pts(Category::SmallStraight, {1, 2, 3, 4, 6}) == 30);
        check("small straight: 3456 + 1, unsorted", pts(Category::SmallStraight, {6, 1, 4, 3, 5}) == 30);
        check("small straight: with a pair inside", pts(Category::SmallStraight, {2, 3, 3, 4, 5}) == 30);
        check("small straight: 1235 + 6 is not", pts(Category::SmallStraight, {1, 2, 3, 5, 6}) == 0);
        check("large straight: 12345", pts(Category::LargeStraight, {5, 4, 3, 2, 1}) == 40);
        check("large straight: 23456", pts(Category::LargeStraight, {2, 3, 4, 5, 6}) == 40);
        check("large straight: 12346 is not", pts(Category::LargeStraight, {1, 2, 3, 4, 6}) == 0);
        check("large straight is also small", pts(Category::SmallStraight, {1, 2, 3, 4, 5}) == 30);
        check("yatze: five alike", pts(Category::Yatze, {4, 4, 4, 4, 4}) == 50);
        check("yatze: not four", pts(Category::Yatze, {4, 4, 4, 4, 3}) == 0);
        check("chance: the sum", pts(Category::Chance, {1, 2, 3, 4, 6}) == 16);
        check("unrolled dice score nothing", pts(Category::Chance, {0, 0, 0, 0, 0}) == 0);
    }

    std::cout << "[rules: card]\n";
    {
        model::Card c;
        check("card: starts all open", !c.full() && c.open(Category::Ones));
        c.box = {3, 6, 9, 12, 15, 18, model::kOpen, model::kOpen, model::kOpen, model::kOpen, model::kOpen, model::kOpen, model::kOpen};
        check("upper bonus: exactly 63 earns 35", c.upper_sum() == 63 && c.upper_bonus() == 35);
        c.box[0] = 2;
        check("upper bonus: 62 earns nothing", c.upper_bonus() == 0);
        c.box = {0, 0, 0, 0, 0, 0, 20, 0, 25, 30, 40, 50, 22};
        c.bonuses = 2;
        check("total: lower + two Yatze bonuses", c.total() == 187 + 200, std::to_string(c.total()));
        check("card: full", c.full());
    }

    std::cout << "[rules: joker]\n";
    {
        using model::kOpen;
        model::Card c;
        const Dice y{4, 4, 4, 4, 4};
        auto o = model::score_options(c, y);
        check("first Yatze: no joker, every open box allowed",
              !o.joker && std::all_of(o.allowed.begin(), o.allowed.end(), [](bool b) { return b; }));

        c.box[model::index(Category::Yatze)] = 50;
        o = model::score_options(c, y);
        check("further Yatze with 50: joker + bonus", o.joker && o.bonus);
        check("joker: its upper box (Fours) is forced",
              o.allowed[model::index(Category::Fours)] &&
              std::count(o.allowed.begin(), o.allowed.end(), true) == 1);
        check("joker: Fours scores 20", o.points[model::index(Category::Fours)] == 20);

        c.box[model::index(Category::Fours)] = 12;
        o = model::score_options(c, y);
        check("joker: Fours filled -> any open lower box",
              o.allowed[model::index(Category::FullHouse)] && o.allowed[model::index(Category::Chance)] &&
              !o.allowed[model::index(Category::Ones)]);
        check("joker: full house in full (25)", o.points[model::index(Category::FullHouse)] == 25);
        check("joker: small straight in full (30)", o.points[model::index(Category::SmallStraight)] == 30);
        check("joker: large straight in full (40)", o.points[model::index(Category::LargeStraight)] == 40);

        for (int i = model::kUpper; i < model::kCategories; ++i)
            if (c.box[i] == kOpen) c.box[i] = 0;
        o = model::score_options(c, y);
        check("joker: lower all filled -> any open upper box for 0",
              o.allowed[model::index(Category::Ones)] && o.points[model::index(Category::Ones)] == 0 &&
              !o.allowed[model::index(Category::Fours)]);

        model::Card z;
        z.box[model::index(Category::Yatze)] = 0;
        o = model::score_options(z, y);
        check("Yatze box holds 0: joker, but no bonus", o.joker && !o.bonus);
    }

    std::cout << "[rng: fate]\n";
    {
        std::uint64_t a = 7, b = 7;
        const Dice d1 = model::draw_five(a), d2 = model::draw_five(b);
        check("same seed, same five", d1 == d2 && a == b);
        check("faces are 1..6", model::rolled(d1));
        // Pinned: this seed gives these dice on every compiler (our own draw).
        std::uint64_t p = 42;
        const std::string first = str(model::draw_five(p));
        std::cout << "        seed 42 -> " << first << "\n";
        check("seed 42 pinned", first == "22115", first);
        std::uint64_t s = 1;
        std::array<int, 7> hist{};
        for (int i = 0; i < 6000; ++i) for (int f : model::draw_five(s)) ++hist[f];
        bool even = true;
        for (int f = 1; f <= 6; ++f) if (hist[f] < 4700 || hist[f] > 5300) even = false;
        check("30000 faces, each within 6% of even", even);
    }

    std::cout << "[game: flow]\n";
    {
        Fix f(2);
        using namespace model::verbs;
        check("new game: round 1, P1, no dice", f.g().round == 1 && f.g().current == 0 && !model::rolled(f.g().dice));
        check("cannot hold before rolling", !toggle_hold(f.s, f.j, 0));
        check("cannot score before rolling", !score(f.s, f.j, Category::Chance));
        check("roll 1", roll(f.s, f.j) && f.g().rolls_used == 1 && model::rolled(f.g().dice));
        check("hold die 1", toggle_hold(f.s, f.j, 0) && f.g().held[0]);
        const int kept = f.g().dice[0];
        check("roll 2 keeps the held die", roll(f.s, f.j) && f.g().dice[0] == kept);
        check("roll 3", roll(f.s, f.j) && f.g().rolls_left() == 0);
        check("no fourth roll", !roll(f.s, f.j));
        check("no holding after the last roll", !toggle_hold(f.s, f.j, 1));
        check("bad die index refused", !toggle_hold(f.s, f.j, 7));
        const int chance = model::sum(f.g().dice);
        check("score Chance", score(f.s, f.j, Category::Chance));
        check("P1's Chance holds the sum", f.g().players[0].card.box[model::index(Category::Chance)] == chance);
        check("turn passes to P2, dice reset, holds cleared",
              f.g().current == 1 && f.g().rolls_used == 0 && !f.g().held[0] && f.g().round == 1);
        roll(f.s, f.j);
        check("P2 cannot score a box P1 filled? (own card -- Chance open)", can_score(f.g(), Category::Chance));
        score(f.s, f.j, Category::Chance);
        check("both played: round 2, P1 again", f.g().round == 2 && f.g().current == 0);
        roll(f.s, f.j);
        check("a filled box is refused", !score(f.s, f.j, Category::Chance));
    }
    {
        Fix f(3, 99);
        using namespace model::verbs;
        int turns = 0;
        while (!f.g().over() && turns < 100) {
            roll(f.s, f.j);
            // First legal box (a joker may force one).
            for (int i = 0; i < model::kCategories; ++i)
                if (score(f.s, f.j, model::category(i))) break;
            ++turns;
        }
        check("3 players: 39 turns end the game", f.g().over() && turns == 39, std::to_string(turns));
        check("round stays at 13", f.g().round == 13);
        check("game over: no roll", !roll(f.s, f.j));
        check("a leader exists", !f.g().leaders().empty());
        check("no stray writes in a whole game", f.s.strays() == 0);
    }

    std::cout << "[undo: the one door; a roll is final]\n";
    {
        using namespace model::verbs;
        struct Verb { std::string name; std::function<bool(Fix&)> prep, act; };
        const std::vector<Verb> table = {
            {"toggle_hold", [](Fix& f) { return roll(f.s, f.j); },
                            [](Fix& f) { return toggle_hold(f.s, f.j, 2); }},
            {"score",       [](Fix& f) { return roll(f.s, f.j); },
                            [](Fix& f) { for (int i = 0; i < model::kCategories; ++i)
                                             if (score(f.s, f.j, model::category(i))) return true;
                                         return false; }},
        };
        for (const auto& v : table) {
            Fix f(2);
            v.prep(f);
            const auto before = f.g();
            const auto steps  = f.j.size();
            const bool ok = v.act(f);
            const auto after = f.g();
            check("gate/" + v.name + ": accepted", ok);
            check("gate/" + v.name + ": through the one door", f.s.strays() == 0);
            check("gate/" + v.name + ": is a step", f.j.size() == steps + 1 && !(before == after));
            f.j.undo();
            check("gate/" + v.name + ": undo puts it back exactly", f.g() == before);
            f.j.redo();
            check("gate/" + v.name + ": redo reproduces it", f.g() == after);
        }
        {
            Fix f;
            f.s.put(model::GameStore::kGame, f.g());
            check("gate: a write around the door is a stray", f.s.strays() == 1);
        }
        {
            // Scott, s002: no undo for rolls. A roll goes through the door (no
            // stray) and then ends undo: nothing before it can come back.
            Fix f;
            roll(f.s, f.j);
            check("roll: through the one door", f.s.strays() == 0);
            check("roll: is final -- nothing to undo after it", !f.j.can_undo() && !f.j.can_redo());
            toggle_hold(f.s, f.j, 0);
            check("roll: a hold after it is undoable", f.j.can_undo());
            roll(f.s, f.j);
            check("roll: the next roll takes the hold out of reach", !f.j.can_undo());
            const auto before = f.g();
            for (int i = 0; i < model::kCategories; ++i)
                if (score(f.s, f.j, model::category(i))) break;
            f.j.undo();
            check("roll: the box taken after the last roll is undoable", f.g() == before);
            f.j.redo();
            roll(f.s, f.j);
            check("roll: the next player's roll ends that too", !f.j.can_undo());
        }
        {
            Fix f;
            roll(f.s, f.j);
            toggle_hold(f.s, f.j, 4);
            check("label: Hold die 5", f.j.undo_label() == "Hold die 5", f.j.undo_label());
            Fix g;
            set_dice(g, {2, 2, 5, 5, 5});
            model::verbs::score(g.s, g.j, Category::FullHouse);
            check("label: Score 25 in Full house", g.j.undo_label() == "Score 25 in Full house", g.j.undo_label());
            model::verbs::new_game(g.s, g.j, {{"A"}});
            check("new game clears undo", !g.j.can_undo() && !g.j.can_redo());
        }
        {
            Fix f;
            set_dice(f, {4, 4, 4, 4, 4});
            model::verbs::score(f.s, f.j, Category::Yatze);
            roll(f.s, f.j);
            set_dice(f, {4, 4, 4, 4, 4});
            const bool forced_other = !model::verbs::score(f.s, f.j, Category::Chance);
            const bool fours = model::verbs::score(f.s, f.j, Category::Fours);
            const auto& c = f.g().players[0].card;
            check("game joker: Chance refused, Fours forced", forced_other && fours);
            check("game joker: +100 banked", c.bonuses == 1 && c.box[model::index(Category::Fours)] == 20);
            f.j.undo();
            check("game joker: undo takes the bonus back too", f.g().players[0].card.bonuses == 0);
        }
    }

    std::cout << "[strategy: the table]\n";
    namespace st = model::strategy;
    st::Table table;
    {
        const auto t0 = std::chrono::steady_clock::now();
        const bool built = table.build();
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.4f in %.1f s", table.start(), secs);
        check("table: built", built && table.ready(), buf);
        // The optimal solitaire game under these rules. Published (Verhoeff,
        // OSYP): 254.59; ours 254.5877 -- see ARCHITECTURE, "the anchor".
        check("table: optimal expected game is 254.59 (+-0.01)", std::fabs(table.start() - 254.59) < 0.01, buf);
        check("table: pinned at 254.5877", std::fabs(table.start() - 254.5877) < 0.0005, buf);
        st::State last;
        last.mask = std::uint16_t(((1u << model::kCategories) - 1) & ~(1u << model::index(Category::Chance)));
        // Only Chance left: re-roll the low dice twice. Exactly 23.3333 (70/3 by
        // hand: one die kept at >=5 on the first reroll, >=4 on the second).
        check("table: only Chance left is worth 23.33 (re-rolling the low dice)",
              std::fabs(table.future(last) - 70.0 / 3.0) < 1e-3, std::to_string(table.future(last)));
        const auto file = std::filesystem::temp_directory_path() / "yatze_selftest_ev.bin";
        st::Table back;
        check("table: save + load round-trips", table.save(file.string()) && back.load(file.string()) &&
              back.start() == table.start() && back.future(last) == table.future(last));
        std::filesystem::remove(file);
        st::Table none;
        check("table: a missing file does not load", !none.load("/nonexistent/yatze.bin") && !none.ready());
    }

    std::cout << "[strategy: the rules agree]\n";
    {
        // The state version of the rules (the table's) against the game's own
        // score_options, on cards from random play -- every roll of five dice.
        int cards = 0, bad = 0;
        std::uint64_t seed = 12345;
        for (int game = 0; game < 30; ++game) {
            Fix f(1, 1000 + game);
            for (int turn = 0; turn < model::kRounds; ++turn) {
                const auto& card = f.g().players[0].card;
                const st::State s = st::state_of(card);
                ++cards;
                for (int a = 1; a <= 6; ++a) for (int b = a; b <= 6; ++b) for (int c = b; c <= 6; ++c)
                for (int d = c; d <= 6; ++d) for (int e = d; e <= 6; ++e) {
                    const Dice dice{a, b, c, d, e};
                    const auto opt = model::score_options(card, dice);
                    const auto mv  = st::moves(s, dice);
                    int legal = 0;
                    for (int i = 0; i < model::kCategories; ++i) legal += opt.allowed[i];
                    if (int(mv.size()) != legal) { ++bad; continue; }
                    for (const auto& m : mv) {
                        const int i = model::index(m.c);
                        const int bonus = opt.bonus ? model::kYatzeBonus : 0;
                        model::Card after = card;
                        after.box[i] = m.points;
                        if (!opt.allowed[i] || opt.points[i] != m.points ||
                            m.gain != after.total() - card.total() + bonus) ++bad;
                    }
                }
                // Advance with a random legal box (and random dice).
                model::verbs::roll(f.s, f.j);
                std::vector<int> open;
                for (int i = 0; i < model::kCategories; ++i)
                    if (model::verbs::can_score(f.g(), model::category(i))) open.push_back(i);
                seed = seed * 6364136223846793005ull + 1442695040888963407ull;
                model::verbs::score(f.s, f.j, model::category(open[(seed >> 33) % open.size()]));
            }
        }
        check("moves() == score_options on every roll of " + std::to_string(cards) + " random cards",
              bad == 0, std::to_string(bad) + " disagreements");
    }

    std::cout << "[strategy: advice]\n";
    {
        model::Card empty;
        // Consistency: playing advise()'s best over every first roll IS the table.
        double sum = 0;
        for (int i = 0; i < 7776; ++i) {
            const Dice d{i % 6 + 1, i / 6 % 6 + 1, i / 36 % 6 + 1, i / 216 % 6 + 1, i / 1296 % 6 + 1};
            const auto a = st::advise(table, empty, d, 2);
            double best = a.boxes.front().expect;
            if (!a.holds.empty()) best = std::max(best, a.holds.front().expect);
            sum += best;
        }
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.4f vs %.4f", sum / 7776.0, table.start());
        check("advise: the best play over all 7776 first rolls averages the table", std::fabs(sum / 7776.0 - table.start()) < 0.01, buf);

        auto a = st::advise(table, empty, {5, 5, 5, 5, 5}, 2);
        check("advise: five 5s on an empty card -> take the Yatze", !a.roll_on && a.boxes.front().c == Category::Yatze);
        a = st::advise(table, empty, {1, 2, 3, 4, 5}, 2);
        check("advise: a large straight on an empty card -> take it", !a.roll_on && a.boxes.front().c == Category::LargeStraight);
        a = st::advise(table, empty, {6, 6, 6, 2, 1}, 2);
        check("advise: three 6s, two rolls left -> roll on, keeping the 6s",
              a.roll_on && a.holds.front().count == 3 && a.holds.front().kept[0] == 6 &&
              a.holds.front().hold[0] && a.holds.front().hold[1] && a.holds.front().hold[2] && !a.holds.front().hold[3]);
        a = st::advise(table, empty, {6, 6, 6, 2, 1}, 0);
        check("advise: no rolls left -> boxes only", a.holds.empty() && !a.roll_on && !a.boxes.empty());
        check("advise: boxes ranked best first", std::is_sorted(a.boxes.begin(), a.boxes.end(),
              [](const st::BoxAdvice& x, const st::BoxAdvice& y) { return x.expect > y.expect; }));
        a = st::advise(table, empty, {6, 6, 6, 2, 1}, 1);
        check("advise: holds are distinct keeps (not all five)", !a.holds.empty() &&
              std::all_of(a.holds.begin(), a.holds.end(), [](const st::HoldAdvice& h) { return h.count < 5; }));
        check("advise: no table -> no advice", st::advise(st::Table{}, empty, {1, 2, 3, 4, 5}, 2).boxes.empty());
    }

    std::cout << "[drawers]\n";
    {
        using Side = model::Drawers::Side;
        model::Drawers d;
        check("drawers: start closed", !d.scrim());
        check("drawers: left opens", d.toggle(Side::Left) && d.is_open(Side::Left));
        check("drawers: right replaces left", d.toggle(Side::Right) && !d.is_open(Side::Left));
        check("drawers: toggle closes", d.toggle(Side::Right) && !d.scrim());
    }

    std::cout << "[shortcuts]\n";
    {
        check("shortcuts: registry populated", !model::shortcut_registry().empty());
        const auto col = model::find_accel_collisions();
        check("shortcuts: no colliding accels", col.empty(), col.empty() ? "clean" : col.front());
        std::vector<std::string> secs;
        for (const auto& r : model::shortcut_registry())
            if (secs.empty() || secs.back() != r.section) secs.push_back(r.section);
        auto sorted = secs;
        std::sort(sorted.begin(), sorted.end());
        check("shortcuts: sections contiguous and A-Z", secs == sorted &&
              std::adjacent_find(secs.begin(), secs.end()) == secs.end());
    }

    std::cout << "-----------------------------------------------\n";
    std::cout << g_pass << " pass / " << g_fail << " fail\n";
    return g_fail == 0 ? 0 : 1;
}
