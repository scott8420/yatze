#pragma once
#include "model/Game.hpp"
#include "widgets/Widgets.hpp"

// StatsPanel -- the RIGHT drawer's content: a report, read when it opens
// (show), never kept live. s001 reports this game's standings; high scores and
// history (persisted) come later.
namespace yatze {

class StatsPanel : public widgets::Box {
public:
    StatsPanel();
    void show(const model::GameState& g);

private:
    widgets::Label m_body;
};

}  // namespace yatze
