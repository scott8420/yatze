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

const char* key(Pace p) { return p == Pace::Quick ? "quick" : "watch"; }

bool parse(const std::string& k, Pace& out) {
    if (k == "watch") { out = Pace::Watch; return true; }
    if (k == "quick") { out = Pace::Quick; return true; }
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
        if (j.contains("sound") && j["sound"].is_boolean()) s.sound = j["sound"].get<bool>();
        Pace p;
        if (parse(j.value("pace", std::string{}), p)) s.pace = p;
        if (j.contains("window") && j["window"].is_object()) {
            const auto& w = j["window"];
            // A size you could not have left it at (tiny, absurd) keeps the default.
            const int ww = w.value("width", 0), hh = w.value("height", 0);
            if (ww >= 480 && ww <= 10000 && hh >= 400 && hh <= 10000) { s.width = ww; s.height = hh; }
            if (w.contains("maximized") && w["maximized"].is_boolean()) s.maximized = w["maximized"].get<bool>();
        }
    } catch (...) {
        return Settings{};
    }
    return s;
}

bool save(const std::string& path, const Settings& s) {
    nlohmann::json j;
    j["version"] = 1;
    j["dice"] = key(s.dice);
    j["sound"] = s.sound;
    j["pace"] = key(s.pace);
    j["window"] = {{"width", s.width}, {"height", s.height}, {"maximized", s.maximized}};
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::ofstream out(path);
    if (!out) return false;
    out << j.dump(2) << "\n";
    return bool(out);
}

}  // namespace yatze::model::settings
