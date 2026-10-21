#pragma once
// model::settings -- small preferences remembered between launches.
// $XDG_CONFIG_HOME/yatze/settings.json (the UI resolves the path).
//   { "version": 1, "dice": "red" }
// Missing or bad -> the defaults. Unknown keys are ignored (room to grow).
#include <string>

namespace yatze::model::settings {

enum class DiceStyle { Red, Ivory };   // Red: white pips (the classic); Ivory: black pips

struct Settings {
    DiceStyle dice = DiceStyle::Red;   // Scott, s004b: "standard dice is red, but ivory too"
    bool operator==(const Settings&) const = default;
};

Settings load(const std::string& path);
bool save(const std::string& path, const Settings& s);

const char* key(DiceStyle d);                 // "red" / "ivory"
bool parse(const std::string& k, DiceStyle& out);

}  // namespace yatze::model::settings
