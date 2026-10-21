#pragma once
// model::table_file -- the players you set up, remembered between launches
// (Scott, s004). A small JSON file; the UI resolves where it lives
// ($XDG_CONFIG_HOME/yatze/table.json) and hands the model a plain path.
//
//   { "version": 1, "seats": [ { "name": "You", "seat": "person" },
//                              { "name": "Computer", "seat": "computer", "level": "medium" } ] }
//
// A missing, unreadable or nonsense file loads as "nothing remembered" (an
// empty vector) -- never a crash, never half a table.
#include "model/Game.hpp"

#include <string>
#include <vector>

namespace yatze::model::table_file {

std::vector<Player> load(const std::string& path);
bool save(const std::string& path, const std::vector<Player>& seats);

}  // namespace yatze::model::table_file
