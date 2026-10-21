#include "Shell.hpp"
#include "Log.hpp"

#include <glibmm/main.h>
#include <gtkmm/editable.h>
#include <gtkmm/textview.h>

// Shell_helpers.cpp -- HELPERS. Paint the window from the model; nothing here
// writes the game.

namespace yatze {

// Several writes in one handler (or an undo) repaint once, after they settle.
void Shell::queue_repaint() {  // helper
    if (m_repaint_queued) return;
    m_repaint_queued = true;
    Glib::signal_idle().connect_once([this] {
        m_repaint_queued = false;
        repaint();
    });
}

void Shell::repaint() {  // helper: dice, card, status, actions
    const auto& g = m_store.state();
    m_dice.set(g.dice, g.held, model::verbs::can_hold(g));
    m_card.set(g);
    m_status.set_text(status_text());
    if (m_act_roll) m_act_roll->set_enabled(model::verbs::can_roll(g));
    m_roll.set_label(g.over() ? "Game over" : g.rolls_used == 0 ? "Roll"
                     : "Roll  (" + std::to_string(g.rolls_left()) + " left)");
    if (m_drawers.is_open(model::Drawers::Side::Right)) m_stats.show(g);
}

std::string Shell::status_text() const {  // helper
    const auto& g = m_store.state();
    if (g.players.empty()) return "";
    if (g.over()) {
        const auto lead = g.leaders();
        const int top = g.players[lead.front()].card.total();
        if (g.players.size() == 1) return "Game over — " + std::to_string(top) + " points";
        if (lead.size() > 1) return "Game over — a tie at " + std::to_string(top);
        return "Game over — " + g.players[lead.front()].name + " wins with " + std::to_string(top);
    }
    std::string who = g.players.size() == 1 ? "" : g.player().name + " — ";
    std::string what = g.rolls_used == 0 ? "roll the dice"
                     : g.rolls_left() == 0 ? "take a box"
                     : "hold, roll again, or take a box";
    return who + "Round " + std::to_string(g.round) + " of 13 — " + what;
}

void Shell::update_undo_ui() {  // helper
    if (m_act_undo) m_act_undo->set_enabled(m_journal.can_undo());
    if (m_act_redo) m_act_redo->set_enabled(m_journal.can_redo());
    const std::string u = m_journal.undo_label(), r = m_journal.redo_label();
    if (m_edit_menu) {
        m_edit_menu->remove_all();
        m_edit_menu->append(u.empty() ? "Undo" : "Undo " + u, "win.undo");
        m_edit_menu->append(r.empty() ? "Redo" : "Redo " + r, "win.redo");
    }
}

// Paint the drawers from m_drawers -- the one place a revealer moves. The
// standings read as the drawer opens; on close the keyboard returns to play.
void Shell::apply_drawers() {  // helper
    using Side = model::Drawers::Side;
    if (m_drawers.is_open(Side::Right)) m_stats.show(m_store.state());
    m_left_drawer.set_reveal_child(m_drawers.is_open(Side::Left));
    m_right_drawer.set_reveal_child(m_drawers.is_open(Side::Right));
    m_scrim.set_visible(m_drawers.scrim());
    if (!m_drawers.scrim()) m_roll.grab_focus();
}

bool Shell::focus_is_text() const {  // helper
    for (const Gtk::Widget* w = get_focus(); w; w = w->get_parent())
        if (dynamic_cast<const Gtk::TextView*>(w) || dynamic_cast<const Gtk::Editable*>(w)) return true;
    return false;
}

}  // namespace yatze
