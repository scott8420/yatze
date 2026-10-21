# Yatzé!

A dice game for GNOME: five dice, thirteen boxes, three rolls a turn.
gtkmm 4 / C++23. Started from the Cairn seed.

**Status: s001 — playable skeleton.** Flat dice, the full scorecard (upper
bonus, Yatzé bonus, forced Joker), hot seat for 1–4 players, undo, a rules
drawer and a standings drawer. Coming: hints, computer players, 3D dice that
tumble and settle.

## Build & run

```
./build.sh
./build/yatze_selftest     # the rules and the game, headless
./build/yatze
```

Deps (Fedora): `gtkmm4.0-devel spdlog-devel cmake gcc-c++`.
Deps (Ubuntu/Debian): `libgtkmm-4.0-dev libspdlog-dev cmake g++`.

## Playing

- **R** or the Roll button rolls every die not held; three rolls a turn.
- Click a die (or press **1–5**) to hold it.
- The scorecard shows what each open box would score; click one to take it.
- **Ctrl+Z / Ctrl+Shift+Z** undo / redo a hold or the box you took. A roll
  is final.
- **F9** rules, **Shift+F9** standings, **Esc** closes. **Ctrl+N** new game;
  menu › New game with… for 1–4 players.
- **Ctrl+?** lists every key.

"Yatzé!" is this game's name; it is not affiliated with Hasbro's Yahtzee®.

## License

MIT -- see LICENSE.
