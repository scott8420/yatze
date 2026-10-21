# Yatzé!

A dice game for GNOME: five dice, thirteen boxes, three rolls a turn.
gtkmm 4 / C++23. Started from the Cairn seed.

**Status: s004.** The full game (upper bonus, Yatzé bonus, forced Joker),
1–6 seats of people and computer players (Easy / Medium / Hard -- Hard plays
optimally), live hints from an exact strategy engine, undo, and the dice on
a 3D table (OpenGL 3.3; flat dice if it is unavailable). Coming: the roll as
a filmed take -- real tumbling physics and a planned camera.

## Build & run

```
./build.sh
./build/yatze_selftest     # the rules and the game, headless
./build/yatze
```

Deps (Fedora): `gtkmm4.0-devel spdlog-devel libepoxy-devel cmake gcc-c++`.
Deps (Ubuntu/Debian): `libgtkmm-4.0-dev libspdlog-dev libepoxy-dev cmake g++`.

## Playing

- **R** or the Roll button rolls every die not held; three rolls a turn.
- Click a die (or press **1–5**) to hold it; it steps forward on a blue ring.
- Menu › **Red dice** / **Ivory dice**.
- The scorecard shows what each open box would score; click one to take it.
- **Ctrl+Z / Ctrl+Shift+Z** undo / redo a hold or the box you took. A roll
  is final.
- **F9** hints and rules (the advised dice glow amber on the table, or the
  advised box is outlined), **Shift+F9** standings, **Esc** closes.
- **Ctrl+N** new game, same players; **Ctrl+Shift+N** choose the players
  (people, or the computer at Easy / Medium / Hard).
- **Ctrl+?** lists every key.

"Yatzé!" is this game's name; it is not affiliated with Hasbro's Yahtzee®.

## License

MIT -- see LICENSE.
