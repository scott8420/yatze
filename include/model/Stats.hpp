#pragma once
// model::stats -- every finished game, remembered; what they add up to.
// GTK-free; the selftest checks the arithmetic and the file.
//
//   $XDG_DATA_HOME/yatze/history.json   (the UI resolves the path)
//   { "version": 1, "games": [ { "when": "2026-10-21 16:40",
//        "seats": [ { "name": "You", "seat": "person", "total": 254,
//                     "yatzes": 1, "bonus": true }, ... ] }, ... ] }
//
// Stats are kept per NAME, so a person's record follows them from table to
// table. A computer seat is its name plus its level ("Computer (Hard)"), so
// Easy and Hard are not averaged together.
#include "model/Game.hpp"

#include <string>
#include <vector>

namespace yatze::model::stats {

struct SeatResult {
    std::string name;
    Seat        seat  = Seat::Human;
    Level       level = Level::Medium;
    int         total = 0;
    int         yatzes = 0;         // the 50 plus each +100 bonus
    bool        bonus = false;      // made the upper bonus
    bool operator==(const SeatResult&) const = default;
};

struct GameRecord {
    std::string             when;   // local "YYYY-MM-DD HH:MM"
    std::vector<SeatResult> seats;
    bool operator==(const GameRecord&) const = default;
    int  best() const;              // the winning total
    bool won(std::size_t seat) const;   // ties all win
};

struct PlayerStats {
    std::string who;                // the key: name, or "name (Level)" for a computer
    bool        computer = false;
    int         games = 0, wins = 0, best = 0, yatzes = 0, bonuses = 0;
    double      average = 0;
};

// s015: a player over time -- every total in the order played, and what the
// recent games say next to the whole record.
struct Trend {
    std::vector<int> totals;        // oldest first
    int    games = 0, best = 0, yatzes = 0, bonuses = 0;
    double average = 0;             // all games
    double recent = 0;              // the last kRecent games (all of them if fewer)
    double bonus_rate = 0;          // share of games with the upper bonus, 0..1
    static constexpr int kRecent = 10;
};

struct HighScore {
    int         total = 0;
    std::string who, when;
};

class History {
public:
    static constexpr std::size_t kMax = 2000;   // oldest dropped beyond this

    void add(GameRecord g);
    void clear() { m_games.clear(); }
    const std::vector<GameRecord>& games() const { return m_games; }

    std::vector<PlayerStats> players() const;          // most games first
    std::vector<HighScore>   top(std::size_t n = 10) const;
    // The best total `who` had BEFORE the last game (0 if none) -- for "a new best!".
    int previous_best(const std::string& who) const;
    Trend trend(const std::string& who) const;         // s015

    bool load(const std::string& path);   // false (and empty) if missing / bad
    bool save(const std::string& path) const;

private:
    std::vector<GameRecord> m_games;
};

std::string key(const SeatResult& s);                 // "You", "Computer (Hard)"
GameRecord  record(const GameState& g, const std::string& when);

}  // namespace yatze::model::stats
