#include "model/Game.hpp"

#include <algorithm>
#include <random>

namespace yatze::model {

// ── GameState ───────────────────────────────────────────────────────────────
bool GameState::over() const {
    if (players.empty()) return false;
    return std::all_of(players.begin(), players.end(), [](const Player& p) { return p.card.full(); });
}

std::vector<int> GameState::leaders() const {
    std::vector<int> out;
    int best = -1;
    for (int i = 0; i < int(players.size()); ++i) {
        const int t = players[i].card.total();
        if (t > best) { best = t; out = {i}; }
        else if (t == best) out.push_back(i);
    }
    return out;
}

// ── the draw ────────────────────────────────────────────────────────────────
namespace {
std::uint64_t splitmix64(std::uint64_t& x) {
    std::uint64_t z = (x += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
int face(std::uint64_t& x) {
    // Rejection keeps the six faces exactly even (2^64 is not a multiple of 6).
    constexpr std::uint64_t lim = ~0ull - (~0ull % 6);
    for (;;) {
        const std::uint64_t r = splitmix64(x);
        if (r < lim) return int(r % 6) + 1;
    }
}
}  // namespace

Dice draw_five(std::uint64_t& state) {
    Dice d{};
    for (int& f : d) f = face(state);
    return d;
}

// ── the store ───────────────────────────────────────────────────────────────
std::optional<GameState> GameStore::get(const Id& id) const {
    if (id != kGame) return std::nullopt;
    return m_state;
}

void GameStore::put(const Id& id, const std::optional<GameState>& v) {
    if (id != kGame || !v) return;               // the one record is never removed
    if (m_door && !m_door()) ++m_strays;         // a write around the door
    m_state = *v;
    if (m_changed) m_changed();
}

void GameStore::load(GameState s) {
    m_state = std::move(s);
    if (m_changed) m_changed();
}

// ── verbs ───────────────────────────────────────────────────────────────────
namespace verbs {

bool can_roll(const GameState& g)  { return !g.players.empty() && !g.over() && g.rolls_left() > 0; }
bool can_hold(const GameState& g)  { return !g.over() && g.rolls_used > 0 && g.rolls_left() > 0; }
bool can_score(const GameState& g, Category c) {
    if (g.players.empty() || g.over() || g.rolls_used == 0) return false;
    return score_options(g.player().card, g.dice).allowed[index(c)];
}

void new_game(GameStore& s, GameJournal& j, std::vector<Player> players, std::uint64_t seed) {
    GameState g;
    for (auto& p : players) p.card = Card{};
    g.players = std::move(players);
    g.rng     = seed ? seed : (std::uint64_t(std::random_device{}()) << 32) ^ std::random_device{}();
    s.load(std::move(g));
    j.clear();
}

bool roll(GameStore& s, GameJournal& j) {
    if (!can_roll(s.state())) return false;
    const int n = s.state().rolls_used + 1;
    return j.run(n == 1 ? "Roll" : "Roll " + std::to_string(n), {GameStore::kGame}, [&] {
        GameState g = s.state();
        const Dice fate = draw_five(g.rng);       // always five: fate per slot
        for (int i = 0; i < 5; ++i)
            if (!g.held[i]) g.dice[i] = fate[i];
        ++g.rolls_used;
        s.put(GameStore::kGame, g);
    });
}

bool toggle_hold(GameStore& s, GameJournal& j, int die) {
    if (die < 0 || die > 4 || !can_hold(s.state())) return false;
    const bool now = !s.state().held[die];
    const std::string label = std::string(now ? "Hold" : "Release") + " die " + std::to_string(die + 1);
    return j.run(label, {GameStore::kGame}, [&] {
        GameState g = s.state();
        g.held[die] = now;
        s.put(GameStore::kGame, g);
    });
}

bool score(GameStore& s, GameJournal& j, Category c) {
    if (!can_score(s.state(), c)) return false;
    const Scoring opt = score_options(s.state().player().card, s.state().dice);
    const int pts = opt.points[index(c)];
    const std::string label = "Score " + std::to_string(pts) + " in " + std::string(name(c));
    return j.run(label, {GameStore::kGame}, [&] {
        GameState g = s.state();
        Card& card = g.players[g.current].card;
        card.box[index(c)] = pts;
        if (opt.bonus) ++card.bonuses;
        // The turn ends: next seat, a new round after the last seat, fresh dice.
        g.current = (g.current + 1) % int(g.players.size());
        if (g.current == 0 && g.round < kRounds) ++g.round;
        g.dice       = Dice{};
        g.held       = {};
        g.rolls_used = 0;
        s.put(GameStore::kGame, g);
    });
}

}  // namespace verbs
}  // namespace yatze::model
