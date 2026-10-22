#include "model/Stats.hpp"
#include "model/Computer.hpp"

#include "json.hpp"   // third_party/ (nlohmann, MIT)

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>

namespace yatze::model::stats {

std::string key(const SeatResult& s) {
    if (s.seat == Seat::Human) return s.name;
    return s.name + " (" + computer::level_name(s.level) + ")";
}

int GameRecord::best() const {
    int b = 0;
    for (const auto& s : seats) b = std::max(b, s.total);
    return b;
}

bool GameRecord::won(std::size_t i) const {
    return i < seats.size() && seats.size() > 1 && seats[i].total == best();
}

GameRecord record(const GameState& g, const std::string& when) {
    GameRecord r;
    r.when = when;
    for (const auto& p : g.players) {
        SeatResult s;
        s.name   = p.name;
        s.seat   = p.seat;
        s.level  = p.seat == Seat::Computer ? p.level : Level::Medium;   // a person has no level
        s.total  = p.card.total();
        s.yatzes = (p.card.box[index(Category::Yatze)] == 50 ? 1 : 0) + p.card.bonuses;
        s.bonus  = p.card.upper_bonus() > 0;
        r.seats.push_back(s);
    }
    return r;
}

void History::add(GameRecord g) {
    m_games.push_back(std::move(g));
    if (m_games.size() > kMax) m_games.erase(m_games.begin(), m_games.begin() + long(m_games.size() - kMax));
}

std::vector<PlayerStats> History::players() const {
    std::map<std::string, PlayerStats> by;
    for (const auto& g : m_games)
        for (std::size_t i = 0; i < g.seats.size(); ++i) {
            const auto& s = g.seats[i];
            auto& p = by[key(s)];
            p.who = key(s);
            p.computer = s.seat == Seat::Computer;
            p.average = (p.average * p.games + s.total) / (p.games + 1);
            ++p.games;
            if (g.won(i)) ++p.wins;
            p.best = std::max(p.best, s.total);
            p.yatzes += s.yatzes;
            if (s.bonus) ++p.bonuses;
        }
    std::vector<PlayerStats> out;
    for (auto& [k, v] : by) out.push_back(v);
    std::stable_sort(out.begin(), out.end(), [](const PlayerStats& a, const PlayerStats& b) {
        if (a.computer != b.computer) return !a.computer;   // people first
        return a.games > b.games;
    });
    return out;
}

Trend History::trend(const std::string& who) const {
    Trend t;
    for (const auto& g : m_games)
        for (const auto& s : g.seats) {
            if (key(s) != who) continue;
            t.totals.push_back(s.total);
            t.best = std::max(t.best, s.total);
            t.yatzes += s.yatzes;
            if (s.bonus) ++t.bonuses;
        }
    t.games = int(t.totals.size());
    if (t.games == 0) return t;
    double sum = 0;
    for (int v : t.totals) sum += v;
    t.average = sum / t.games;
    const int n = std::min(t.games, Trend::kRecent);
    double rs = 0;
    for (int i = t.games - n; i < t.games; ++i) rs += t.totals[std::size_t(i)];
    t.recent = rs / n;
    t.bonus_rate = double(t.bonuses) / t.games;
    return t;
}

std::vector<HighScore> History::top(std::size_t n) const {
    std::vector<HighScore> all;
    for (const auto& g : m_games)
        for (const auto& s : g.seats) all.push_back({s.total, key(s), g.when});
    // Highest first; among equals, the earlier game keeps its place.
    std::stable_sort(all.begin(), all.end(), [](const HighScore& a, const HighScore& b) { return a.total > b.total; });
    if (all.size() > n) all.resize(n);
    return all;
}

int History::previous_best(const std::string& who) const {
    int b = 0;
    for (std::size_t g = 0; g + 1 < m_games.size(); ++g)
        for (const auto& s : m_games[g].seats)
            if (key(s) == who) b = std::max(b, s.total);
    return b;
}

namespace {
const char* lvl(Level l) { return l == Level::Easy ? "easy" : l == Level::Hard ? "hard" : "medium"; }
}

bool History::save(const std::string& path) const {
    nlohmann::json j;
    j["version"] = 1;
    j["games"] = nlohmann::json::array();
    for (const auto& g : m_games) {
        nlohmann::json jg;
        jg["when"] = g.when;
        jg["seats"] = nlohmann::json::array();
        for (const auto& s : g.seats) {
            nlohmann::json js;
            js["name"] = s.name;
            js["seat"] = s.seat == Seat::Computer ? "computer" : "person";
            if (s.seat == Seat::Computer) js["level"] = lvl(s.level);
            js["total"] = s.total;
            js["yatzes"] = s.yatzes;
            js["bonus"] = s.bonus;
            jg["seats"].push_back(js);
        }
        j["games"].push_back(jg);
    }
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    // Write beside, then rename: a crash mid-write never loses the history.
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp);
        if (!out) return false;
        out << j.dump(1) << "\n";
        if (!out) return false;
    }
    std::filesystem::rename(tmp, path, ec);
    return !ec;
}

bool History::load(const std::string& path) {
    m_games.clear();
    std::ifstream in(path);
    if (!in) return false;
    try {
        const auto j = nlohmann::json::parse(in);
        if (j.value("version", 0) != 1 || !j.contains("games")) return false;
        std::vector<GameRecord> games;
        for (const auto& jg : j["games"]) {
            GameRecord g;
            g.when = jg.value("when", std::string{});
            for (const auto& js : jg["seats"]) {
                SeatResult s;
                s.name = js.value("name", std::string{});
                s.seat = js.value("seat", std::string{"person"}) == "computer" ? Seat::Computer : Seat::Human;
                const auto l = js.value("level", std::string{"medium"});
                s.level = l == "easy" ? Level::Easy : l == "hard" ? Level::Hard : Level::Medium;
                s.total = js.value("total", 0);
                s.yatzes = js.value("yatzes", 0);
                s.bonus = js.value("bonus", false);
                g.seats.push_back(s);
            }
            if (!g.seats.empty()) games.push_back(std::move(g));
        }
        m_games = std::move(games);
        return true;
    } catch (...) {
        m_games.clear();
        return false;
    }
}

}  // namespace yatze::model::stats
