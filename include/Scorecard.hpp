#pragma once
#include "model/Game.hpp"
#include "widgets/Widgets.hpp"

#include <sigc++/signal.h>

// Scorecard -- the card, always on screen: one column per player, the box rows,
// the subtotals. In the current player's column every box the rules allow for
// this roll is a button showing what it WOULD score; a click asks to take it
// (signal_pick). Like the dice row, it only shows the model -- set() rebuilds
// the grid from a GameState (13 rows x a few players: cheap, and never stale).
namespace yatze {

class Scorecard : public widgets::Grid {
public:
    Scorecard();
    void set(const model::GameState& g);
    sigc::signal<void(model::Category)>& signal_pick() { return m_sig_pick; }

private:
    void clear();
    sigc::signal<void(model::Category)> m_sig_pick;
};

}  // namespace yatze
