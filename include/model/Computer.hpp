#pragma once
// model::computer -- how a computer seat plays. GTK-free; the selftest plays
// hundreds of whole games per level with it.
//
// ── one step at a time ─────────────────────────────────────────────────────
// decide() looks at the game and returns the NEXT single thing the computer
// does: roll, hold / release one die, or take a box. The UI applies one step
// per tick, so you watch it hold the 6s one by one and roll. The plan behind a
// step depends only on the card, the dice and the rolls left (never on which
// dice are already held), so it stays the same from tick to tick.
//
// ── the three levels ───────────────────────────────────────────────────────
//   Hard   -- the strategy table's play: optimal for its own card (~254.6).
//   Medium -- the same engine, but about one decision in four, if its
//             runner-up is close (within 8 points of final score), it takes
//             that instead: a strong player's slip, not a blunder. The slips
//             are seeded by the position, not a clock, so a game replays.
//   Easy   -- greedy. Takes a straight, full house or Yatze the moment it has
//             one; otherwise keeps its most common face and rolls; at the end,
//             takes the most points, dumping a zero where it hurts least.
#include "model/Game.hpp"
#include "model/Strategy.hpp"

namespace yatze::model::computer {

struct Step {
    enum class Kind { None, Roll, Hold, Score } kind = Kind::None;
    int      die = -1;                     // Hold: which die to toggle
    Category box = Category::Chance;       // Score: which box
};

// Medium and Hard need a ready table; without one they return None (wait).
// Easy never needs it.
Step decide(const GameState& g, const strategy::Table* table);

// Apply one step through the verbs (the one door). False if refused.
bool apply(GameStore& s, GameJournal& j, const Step& step);

const char* level_name(Level l);   // "Easy" / "Medium" / "Hard"

}  // namespace yatze::model::computer
