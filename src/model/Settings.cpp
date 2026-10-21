#include "model/Settings.hpp"

#include "json.hpp"   // third_party/ (nlohmann, MIT)

#include <filesystem>
#include <fstream>

namespace yatze::model::settings {

const char* key(DiceStyle d) { return d == DiceStyle::Ivory ? "ivory" : "red"; }

bool parse(const std::string& k, DiceStyle& out) {
    if (k == "red")   { out = DiceStyle::Red;   return true; }
    if (k == "ivory") { out = DiceStyle::Ivory; return true; }
    return false;
}

Settings load(const std::string& path) {
    Settings s;
    std::ifstream in(path);
    if (!in) return s;
    try {
        const auto j = nlohmann::json::parse(in);
        if (j.value("version", 0) != 1) return s;
        DiceStyle d;
        if (parse(j.value("dice", std::string{}), d)) s.dice = d;
    } catch (...) {
        return Settings{};
    }
    return s;
}

bool save(const std::string& path, const Settings& s) {
    nlohmann::json j;
    j["version"] = 1;
    j["dice"] = key(s.dice);
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::ofstream out(path);
    if (!out) return false;
    out << j.dump(2) << "\n";
    return bool(out);
}

}  // namespace yatze::model::settings
