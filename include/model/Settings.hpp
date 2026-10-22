#pragma once
// model::settings -- small preferences remembered between launches.
// $XDG_CONFIG_HOME/yatze/settings.json (the UI resolves the path).
//   { "version": 1, "dice": "red", "sound": true, "pace": "watch",
//     "window": { "width": 920, "height": 760, "maximized": false } }
// Missing or bad -> the defaults. Unknown keys are ignored (room to grow).
#include <string>

namespace yatze::model::settings {

enum class DiceStyle { Red, Ivory };
// s017: how the computer's turns play. Watch: every roll in full (Scott, s008:
// "play in full"); Quick: its rolls and pauses about twice as fast.
enum class Pace { Watch, Quick };   // Red: white pips (the classic); Ivory: black pips

struct Settings {
    DiceStyle dice = DiceStyle::Red;   // Scott, s004b: "standard dice is red, but ivory too"
    bool      sound = true;            // s009: the clatter of the take
    Pace      pace  = Pace::Watch;     // s017
    int       width = 920, height = 760;   // s017: the window, as you left it
    bool      maximized = false;
    bool operator==(const Settings&) const = default;
};

Settings load(const std::string& path);
bool save(const std::string& path, const Settings& s);

const char* key(DiceStyle d);                 // "red" / "ivory"
bool parse(const std::string& k, DiceStyle& out);
const char* key(Pace p);                      // "watch" / "quick"
bool parse(const std::string& k, Pace& out);

}  // namespace yatze::model::settings
