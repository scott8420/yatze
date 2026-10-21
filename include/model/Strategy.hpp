#pragma once
// model::strategy -- the engine behind the hints (s002) and the computer
// player (s003). GTK-free; the selftest checks it.
//
// ── two parts ──────────────────────────────────────────────────────────────
// 1. THE TABLE: for every card state between turns, the expected score still
//    to come under optimal solitaire play. A state is what matters for the
//    future and nothing else: which boxes are filled (13 bits), the upper
//    total capped at 63 (the bonus only cares about reaching 63), and whether
//    the Yatze box holds 50 (the +100s depend on it). 2^13 x 64 x 2 slots,
//    unreachable ones skipped. Built backwards from the full card; the start
//    value is the optimal expected game, ~254.59 -- the selftest's anchor.
//    Built once (a few seconds, all cores), then cached to disk.
//
// 2. THE TURN: with the table as the value of every card the turn can end
//    in, the turn itself is solved exactly: every outcome of five dice (252),
//    every keep (462), up to three rolls. advise() answers "what now?" for
//    the dice on the table -- every legal box and every distinct hold,
//    ranked by expected final score.
//
// The table's legality and points come from the same rules as the game
// (model::Rules); the selftest cross-checks the state version against
// score_options on random cards so the two can never disagree.
#include "model/Rules.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace yatze::model::strategy {

// ── the state between turns ─────────────────────────────────────────────────
struct State {
    std::uint16_t mask  = 0;     // bit i set = box i filled
    int           upper = 0;     // upper total, capped at 63
    bool          y50   = false; // the Yatze box holds 50
};
State state_of(const Card& c);
inline constexpr int kStates = (1 << kCategories) * 64 * 2;
inline int slot(const State& s) { return ((int(s.mask) << 6) | s.upper) << 1 | int(s.y50); }

// ── the table ───────────────────────────────────────────────────────────────
class Table {
public:
    // Build every reachable state. `threads` 0 = all cores. `cancel` may be
    // set from another thread to stop early (returns false). `progress`
    // (0..1) is updated as levels finish.
    bool build(int threads = 0, const std::atomic<bool>* cancel = nullptr,
               std::atomic<float>* progress = nullptr);
    bool save(const std::string& path) const;
    bool load(const std::string& path);   // false if missing / wrong size / wrong version

    bool  ready() const { return !m_f.empty(); }
    float future(const State& s) const { return m_f[std::size_t(slot(s))]; }
    float start() const { return future(State{}); }   // the optimal expected game

private:
    std::vector<float> m_f;
};

// ── what the rules allow, in state terms ────────────────────────────────────
// For a roll in a state: each legal box with what it adds now (points + any
// upper bonus it completes + any +100) and the state it leads to.
struct Move {
    Category c;
    int      points;  // what the box records
    int      gain;    // points + the upper bonus it completes + any +100
    State    next;
};
std::vector<Move> moves(const State& s, const Dice& d);

// ── advice for the dice on the table ────────────────────────────────────────
struct BoxAdvice {
    Category c;
    int      points;  // what the box shows
    int      gain;    // points + bonuses earned by taking it now
    double   expect;  // expected FINAL score if taken now
};
struct HoldAdvice {
    std::array<bool, 5> hold{};   // which dice (positions on the table) to keep
    Dice                kept{};   // the kept faces, sorted, 0-padded
    int                 count = 0;
    double              expect;   // expected final score holding these and rolling on
};
struct Advice {
    std::vector<BoxAdvice>  boxes;   // legal boxes, best first
    std::vector<HoldAdvice> holds;   // distinct holds, best first (empty: no rolls left)
    bool                    roll_on = false;  // the best hold beats the best box
};
// `card` is the player's card; `rolls_left` 0..2 after at least one roll.
Advice advise(const Table& t, const Card& card, const Dice& dice, int rolls_left);

}  // namespace yatze::model::strategy
