#include "model/Computer.hpp"

#include <algorithm>

namespace yatze::model::computer {

namespace {

// What the computer means to do with these dice: take a box, or keep some
// dice (a hold mask) and roll.
struct Plan {
    bool                score = false;
    Category            box   = Category::Chance;
    std::array<bool, 5> keep{};
};

std::uint64_t mix(std::uint64_t z) {
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// A "mistake" roll for Medium, seeded by the position (card, dice, rolls).
bool slips(const Card& card, const Dice& d, int rolls_left, int round) {
    std::uint64_t h = std::uint64_t(strategy::slot(strategy::state_of(card)));
    for (int f : d) h = h * 7 + std::uint64_t(f);
    h = h * 4 + std::uint64_t(rolls_left);
    h = h * 16 + std::uint64_t(round);
    return mix(h + 0x9E3779B97F4A7C15ull) % 4 == 0;
}

Plan plan_table(const strategy::Table& t, const Card& card, const Dice& d, int rolls_left,
                bool second_best) {
    const auto a = strategy::advise(t, card, d, rolls_left);
    // Every option, best first: holds (rolling on) and boxes, by final score.
    struct Opt { double v; bool score; Category c; std::array<bool, 5> keep; };
    std::vector<Opt> opts;
    for (const auto& b : a.boxes) opts.push_back({b.expect, true, b.c, {}});
    for (const auto& h : a.holds) opts.push_back({h.expect, false, Category::Chance, h.hold});
    std::stable_sort(opts.begin(), opts.end(), [](const Opt& x, const Opt& y) { return x.v > y.v; });
    // A slip is a PLAUSIBLE mistake: the runner-up, and only when it is close
    // (within kSlipGap points of final score). A strong player's error, not a blunder.
    constexpr double kSlipGap = 8.0;
    const bool slip = second_best && opts.size() > 1 && opts[0].v - opts[1].v <= kSlipGap;
    const Opt& o = slip ? opts[1] : opts[0];
    Plan p;
    p.score = o.score;
    p.box   = o.c;
    p.keep  = o.keep;
    return p;
}

Plan plan_easy(const Card& card, const Dice& d, int rolls_left) {
    const auto opt = score_options(card, d);
    auto can = [&](Category c) { return opt.allowed[index(c)] && opt.points[index(c)] > 0; };
    Plan p;
    // A made hand is taken at once.
    for (Category c : {Category::Yatze, Category::LargeStraight, Category::SmallStraight, Category::FullHouse})
        if (can(c)) { p.score = true; p.box = c; return p; }
    if (rolls_left > 0) {
        // Keep the most common face (ties: the higher face) and roll the rest.
        std::array<int, 7> n{};
        for (int f : d) ++n[f];
        int face = 6;
        for (int f = 6; f >= 1; --f) if (n[f] > n[face]) face = f;
        for (int i = 0; i < 5; ++i) p.keep[i] = d[i] == face;
        return p;
    }
    // Last roll: the most points; a zero goes where it hurts least.
    int best = -1;
    for (int i = 0; i < kCategories; ++i)
        if (opt.allowed[i] && opt.points[i] > best) { best = opt.points[i]; p.box = category(i); }
    if (best == 0) {
        for (Category c : {Category::Ones, Category::Twos, Category::Yatze, Category::Threes,
                           Category::LargeStraight, Category::FourKind, Category::SmallStraight,
                           Category::FullHouse, Category::Fours, Category::Fives, Category::ThreeKind,
                           Category::Sixes, Category::Chance})
            if (opt.allowed[index(c)]) { p.box = c; break; }
    }
    p.score = true;
    return p;
}

}  // namespace

const char* level_name(Level l) {
    switch (l) {
        case Level::Easy:   return "Easy";
        case Level::Medium: return "Medium";
        case Level::Hard:   return "Hard";
    }
    return "?";
}

Step decide(const GameState& g, const strategy::Table* table) {
    Step s;
    if (g.players.empty() || g.over()) return s;
    const Player& me = g.player();
    if (me.seat != Seat::Computer) return s;
    if (g.rolls_used == 0) { s.kind = Step::Kind::Roll; return s; }

    Plan p;
    if (me.level == Level::Easy) {
        p = plan_easy(me.card, g.dice, g.rolls_left());
    } else {
        if (!table || !table->ready()) return s;   // wait for the odds
        const bool slip = me.level == Level::Medium && slips(me.card, g.dice, g.rolls_left(), g.round);
        p = plan_table(*table, me.card, g.dice, g.rolls_left(), slip);
    }
    if (p.score || g.rolls_left() == 0) {
        s.kind = Step::Kind::Score;
        s.box  = p.box;
        if (!p.score) {   // no rolls left but planned a keep: cannot happen; take the best legal box
            const auto opt = score_options(me.card, g.dice);
            for (int i = 0; i < kCategories; ++i) if (opt.allowed[i]) { s.box = category(i); break; }
        }
        return s;
    }
    for (int i = 0; i < 5; ++i)
        if (g.held[i] != p.keep[i]) { s.kind = Step::Kind::Hold; s.die = i; return s; }
    s.kind = Step::Kind::Roll;
    return s;
}

bool apply(GameStore& st, GameJournal& j, const Step& step) {
    switch (step.kind) {
        case Step::Kind::Roll:  return verbs::roll(st, j);
        case Step::Kind::Hold:  return verbs::toggle_hold(st, j, step.die);
        case Step::Kind::Score: return verbs::score(st, j, step.box);
        case Step::Kind::None:  return false;
    }
    return false;
}

}  // namespace yatze::model::computer
