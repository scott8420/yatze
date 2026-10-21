#pragma once
#include "DiceRow.hpp"
#include "Engine.hpp"
#include "HintPanel.hpp"
#include "Scorecard.hpp"
#include "StatsPanel.hpp"
#include "model/Drawers.hpp"
#include "model/Game.hpp"
#include "widgets/Widgets.hpp"

#include <gtkmm/applicationwindow.h>
#include <giomm/menu.h>
#include <giomm/simpleaction.h>

#include <memory>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Shell -- the game window. A SPLIT CLASS (Cairn): the header is the index,
// each method's `// category:` tag names the file its body lives in.
//
//   Zones    -- construction: header, play area, scorecard, drawers, menu.
//               src/Shell_zones.cpp
//   Bindings -- actions, keys, the model <-> journal <-> UI wiring.
//               src/Shell_bindings.cpp
//   Handlers -- on_*: what a user action does. Each game verb is ONE call into
//               model::verbs (which goes through the journal) -- a handler
//               never writes the game itself.
//               src/Shell_handlers.cpp
//   Helpers  -- repaint, undo labels, drawers' paint, status text.
//               src/Shell_helpers.cpp
//   Glue     -- ctor + build_ui.  src/Shell.cpp
//
// Finding things
//   "Where is the window laid out?"     -> Shell_zones.cpp    (build_shell)
//   "What does Roll do?"                -> Shell_handlers.cpp (on_roll)
//   "Where do the 1-5 keys go?"         -> Shell_bindings.cpp (bind_actions)
//   "Who repaints after a move?"        -> Shell_helpers.cpp  (queue_repaint / repaint)
//   "Where do the hints come from?"     -> Engine.{hpp,cpp} (table) + HintPanel::show (advice)
//   "Where does Ctrl+Z go?"             -> Shell_bindings.cpp (the window's undo keys)
// ─────────────────────────────────────────────────────────────────────────────
namespace yatze {

class AboutWindow;
class ShortcutsDialog;

class Shell : public Gtk::ApplicationWindow {
public:
    Shell();
    ~Shell() override;

    void build_ui();                             // category: glue

private:
    // ── zones ──────────────────────────────────────────────────────────────
    void build_shell();                          // category: zone: header + play area + scorecard
    void build_drawers();                        // category: zone: overlay, two drawers, tabs, scrim
    Glib::RefPtr<Gio::Menu> build_menu();        // category: zone: hamburger model

    // ── bindings ───────────────────────────────────────────────────────────
    void bind_actions();                         // category: bindings: actions, keys, wiring

    // ── handlers ───────────────────────────────────────────────────────────
    void on_new_game(int players);               // category: handler: 0 = same table again
    void on_roll();                              // category: handler: R / button
    void on_hold(int die);                       // category: handler: 1-5 / click a die
    void on_score(model::Category c);            // category: handler: click a box
    void on_undo();                              // category: handler: Ctrl+Z / menu
    void on_redo();                              // category: handler: Ctrl+Shift+Z, Ctrl+Y / menu
    void on_toggle_left_drawer();                // category: handler: tab / F9 / menu
    void on_toggle_right_drawer();               // category: handler: tab / Shift+F9 / menu
    void on_close_drawers();                     // category: handler: scrim click / Esc
    void on_about();                             // category: handler
    void on_shortcuts();                         // category: handler

    // ── helpers ────────────────────────────────────────────────────────────
    void queue_repaint();                        // category: helper: one repaint per idle
    void repaint();                              // category: helper: dice, card, status, actions
    std::string status_text() const;             // category: helper: whose turn / who won
    void update_undo_ui();                       // category: helper: labels + grey
    void apply_drawers();                        // category: helper: revealers + scrim
    void refresh_hints();                        // category: helper: the advice, if the drawer is open
    bool focus_is_text() const;                  // category: helper: a text box keeps its own Ctrl+Z

    // ── the model ──────────────────────────────────────────────────────────
    // Declaration order matters: the store before the journal that holds it.
    model::GameStore   m_store;
    model::GameJournal m_journal{m_store};
    Engine             m_engine;                 // the strategy table (hints; the computer player)
    sigc::connection   m_progress_tick;          // repaints "Working out the odds… N%"
    int                m_table = 1;              // players at the table (hot seat)
    bool               m_repaint_queued = false;

    // ── widgets ────────────────────────────────────────────────────────────
    widgets::Button     m_logo_button;
    widgets::Button     m_new_button;
    widgets::MenuButton m_menu_button;

    widgets::Box        m_content;               // play area | scorecard
    widgets::Box        m_play;
    widgets::Label      m_status;
    DiceRow             m_dice;
    widgets::Button     m_roll;
    widgets::Label      m_said;                  // "Undid: Roll 2"
    Scorecard           m_card;

    // The drawers (Cairn's edge-drawer stone). Base = m_content.
    model::Drawers      m_drawers;
    widgets::Overlay    m_overlay;
    widgets::Box        m_scrim;
    widgets::Button     m_left_tab, m_right_tab;
    widgets::Revealer   m_left_drawer, m_right_drawer;
    HintPanel           m_hints;
    StatsPanel          m_stats;

    Glib::RefPtr<Gio::Menu>         m_edit_menu;
    Glib::RefPtr<Gio::SimpleAction> m_act_undo, m_act_redo, m_act_roll;

    std::unique_ptr<AboutWindow>     m_about;
    std::unique_ptr<ShortcutsDialog> m_shortcuts;
};

}  // namespace yatze
