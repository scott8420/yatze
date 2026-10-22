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
    // interactive = a person is to move; on a computer's turn its options show
    // but cannot be clicked.
    // reveal = false while a roll is still playing: the open boxes stay blank
    // so the card does not tell you the dice before they land.
    void set(const model::GameState& g, bool interactive = true, bool reveal = true);
    // The hint's box (-1 none): outlined amber. Kept across set() calls.
    void set_suggested(int box);
    sigc::signal<void(model::Category)>& signal_pick() { return m_sig_pick; }
    // s014: the cell of a player's box (nullptr if none), and a brief amber
    // flash on it as a score lands (kept across set() until it fades).
    Gtk::Widget* box_cell(int player, int box);
    void flash(int player, int box);

private:
    void clear();
    int  m_suggested = -1;
    int  m_flash_player = -1, m_flash_box = -1;
    sigc::connection m_flash_off;
    sigc::signal<void(model::Category)> m_sig_pick;
};

}  // namespace yatze
