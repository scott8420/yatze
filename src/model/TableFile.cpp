#include "model/TableFile.hpp"

#include "json.hpp"   // third_party/ (nlohmann, MIT), private to the model

#include <filesystem>
#include <fstream>

namespace yatze::model::table_file {

namespace {
const char* level_key(Level l) {
    switch (l) {
        case Level::Easy:   return "easy";
        case Level::Medium: return "medium";
        case Level::Hard:   return "hard";
    }
    return "medium";
}
}  // namespace

std::vector<Player> load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return {};
    try {
        const auto j = nlohmann::json::parse(in);
        if (j.value("version", 0) != 1 || !j.contains("seats") || !j["seats"].is_array()) return {};
        std::vector<Player> out;
        for (const auto& s : j["seats"]) {
            Player p;
            p.name = s.value("name", std::string{});
            if (p.name.empty()) return {};
            const auto seat = s.value("seat", std::string{"person"});
            if (seat == "computer") {
                p.seat = Seat::Computer;
                const auto lv = s.value("level", std::string{"medium"});
                p.level = lv == "easy" ? Level::Easy : lv == "hard" ? Level::Hard : Level::Medium;
            } else if (seat != "person") {
                return {};
            }
            out.push_back(p);
        }
        if (out.empty() || out.size() > 6) return {};
        return out;
    } catch (...) {
        return {};
    }
}

bool save(const std::string& path, const std::vector<Player>& seats) {
    nlohmann::json j;
    j["version"] = 1;
    j["seats"] = nlohmann::json::array();
    for (const auto& p : seats) {
        nlohmann::json s;
        s["name"] = p.name;
        s["seat"] = p.seat == Seat::Computer ? "computer" : "person";
        if (p.seat == Seat::Computer) s["level"] = level_key(p.level);
        j["seats"].push_back(s);
    }
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::ofstream out(path);
    if (!out) return false;
    out << j.dump(2) << "\n";
    return bool(out);
}

}  // namespace yatze::model::table_file
