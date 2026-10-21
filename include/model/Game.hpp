#pragma once
// model::Game -- the game, as ONE record the undo journal pictures whole.
//
// ── one record ─────────────────────────────────────────────────────────────
// A Yatzé game is small (a few cards, five dice), so the store holds exactly
// one record, the whole GameState, and every verb's step is a before / after
// picture of all of it. No per-verb inverse, nothing partial to get wrong.
//
// ── a roll is final ────────────────────────────────────────────────────────
// Scott, s002: no undo for rolls. A roll ends undo (the journal is cleared
// after it): holds and the box you took can be undone, never a roll.
// The random state still lives in the record and each roll draws five faces,
// one per slot (our own splitmix64 draw, not std::uniform_int_distribution),
// so a seed gives the same game on every compiler -- the selftest pins one.
//
// ── the one door ───────────────────────────────────────────────────────────
// Every write goes through Journal::run (verbs below). The store counts a write
// made outside it (a "stray"); the selftest walks every verb and fails on one.
#include "model/Rules.hpp"
#include "model/Undo.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace yatze::model {

enum class Seat { Human, Computer };

struct Player {
    std::string name;
    Seat        seat = Seat::Human;
    Card        card;
    bool operator==(const Player&) const = default;
};

inline constexpr int kRollsPerTurn = 3;
inline constexpr int kRounds       = kCategories;   // 13

struct GameState {
    std::vector<Player>  players;
    int                  current    = 0;      // whose turn
    int                  round      = 1;      // 1..13
    Dice                 dice{};              // 0s until the turn's first roll
    std::array<bool, 5>  held{};
    int                  rolls_used = 0;      // this turn, 0..3
    std::uint64_t        rng        = 0;      // the dice to come (seeded per game)
    bool operator==(const GameState&) const = default;

    bool over() const;                        // every card full
    int  rolls_left() const { return kRollsPerTurn - rolls_used; }
    const Player& player() const { return players[current]; }
    std::vector<int> leaders() const;         // index(es) of the highest total
};

// The draw: five faces from the state, the state advanced. Pure.
Dice draw_five(std::uint64_t& state);

// ── the store the journal pictures ─────────────────────────────────────────
class GameStore {
public:
    using Id    = int;
    using Value = GameState;
    static constexpr Id kGame = 0;

    std::optional<Value> get(const Id& id) const;
    void put(const Id& id, const std::optional<Value>& v);

    const GameState& state() const { return m_state; }
    int strays() const { return m_strays; }

    void set_door(std::function<bool()> f)       { m_door = std::move(f); }
    void set_on_change(std::function<void()> f)  { m_changed = std::move(f); }

    // Loading a fresh game is not a user step: it bypasses the door and the
    // caller clears the journal (a new game has nothing to undo into).
    void load(GameState s);

private:
    GameState              m_state;
    int                    m_strays = 0;
    std::function<bool()>  m_door;
    std::function<void()>  m_changed;
};

using GameJournal = Journal<GameStore>;

// ── the verbs -- each one step through the door, or refused (false) ────────
namespace verbs {
// A new game: players in seat order, a seed (0 = from the system). Clears undo.
void new_game(GameStore& s, GameJournal& j, std::vector<Player> players, std::uint64_t seed = 0);
bool roll(GameStore& s, GameJournal& j);                    // needs a roll left
bool toggle_hold(GameStore& s, GameJournal& j, int die);    // after a roll, before the last
bool score(GameStore& s, GameJournal& j, Category c);       // a legal box; ends the turn

// Whether each would be accepted now (the UI greys from these).
bool can_roll(const GameState& g);
bool can_hold(const GameState& g);
bool can_score(const GameState& g, Category c);
}  // namespace verbs

}  // namespace yatze::model
