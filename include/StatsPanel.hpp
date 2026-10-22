#pragma once
#include "model/Game.hpp"
#include "model/Stats.hpp"
#include "widgets/Widgets.hpp"

// StatsPanel -- the RIGHT drawer: a report, read when it opens (show), never
// kept live. This game's standings; the high scores (top ten, all games);
// and per-player records (games, wins, best, average, Yatzés). s006.
// s015: "Over time" -- for each person with two games or more, their totals
// as a small chart (the average dashed, the last ten in amber) and a line on
// how the recent games compare.
namespace yatze {

class StatsPanel : public widgets::Box {
public:
    StatsPanel();
    // `news` is a line to lead with (game over: who won, any new best).
    void show(const model::GameState& g, const model::stats::History& h, const std::string& news = {});

private:
    widgets::Label m_body;
    widgets::Box   m_page;      // the body, then the trend charts
    widgets::Box   m_trends;
};

}  // namespace yatze
