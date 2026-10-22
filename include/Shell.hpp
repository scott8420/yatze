#pragma once
#include "DiceRow.hpp"
#include "DiceView.hpp"
#include "Engine.hpp"
#include "HintPanel.hpp"
#include "Scorecard.hpp"
#include "StatsPanel.hpp"
#include "ViewOrb.hpp"
#include "model/Drawers.hpp"
#include "model/Game.hpp"
#include "model/Settings.hpp"
#include "model/Stats.hpp"
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
//   "How does the computer play?"       -> model/Computer (decide) + Shell_handlers.cpp (on_computer_step)
//                                          + Shell_helpers.cpp (schedule_computer: the pacing)
//   "Where do the hints come from?"     -> Engine.{hpp,cpp} (table) + HintPanel::show (advice)
//   "Where does Ctrl+Z go?"             -> Shell_bindings.cpp (the window's undo keys)
// ─────────────────────────────────────────────────────────────────────────────
namespace yatze {

class AboutWindow;
class NewGameDialog;
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
    void on_new_game();                          // category: handler: Ctrl+N -- same table again
    void on_new_game_dialog();                   // category: handler: Ctrl+Shift+N -- who plays
    void start_game(std::vector<model::Player> seats);   // category: handler: deal a new game for these seats
    void on_computer_step();                     // category: handler: one paced step of a computer turn
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
    void on_dice_style(const Glib::ustring& k);  // category: handler: menu › Red / Ivory dice
    void on_sound();
    void on_clear_history();                     // category: handler: menu › Clear high scores… (confirm)

    // ── helpers ────────────────────────────────────────────────────────────
    std::string settings_file() const;           // category: helper: $XDG_CONFIG_HOME/yatze/settings.json
    void apply_settings();
    std::string history_file() const;            // category: helper: $XDG_DATA_HOME/yatze/history.json
    void check_game_over();                      // category: helper: record a finished game, once                       // category: helper: the dice style into both dice views
    void announce_turn();                        // category: helper: the turn card (s013)
    void fly_score(int player, int box, int points);  // category: helper: the score flies to its box (s014)
    static double test_speed();                  // YATZE_SPEED (testing only)
    static std::string dice_words(const model::GameState& g, bool rolling);  // category: helper (s018)
    bool computer_quick() const;                 // category: helper (s017)
    static constexpr double kQuickRate = 2.2;    // Quick pace: the computer's turn this much faster
    void on_pace(const Glib::ustring& k);        // handler: menu › Computer pace
    void remember_window();                      // helper: the window's size, saved on close (s017)
    std::string table_file() const;              // category: helper: $XDG_CONFIG_HOME/yatze/table.json
    bool human_turn() const;                     // category: helper: a person is to move (input allowed)
    void schedule_computer();                    // category: helper: queue the next computer step, paced
    void queue_repaint();                        // category: helper: one repaint per idle
    void repaint();                              // category: helper: dice, card, status, actions
    std::string status_text() const;             // category: helper: whose turn / who won
    void update_undo_ui();                       // category: helper: labels + grey
    void apply_drawers();                        // category: helper: revealers + scrim
    void refresh_hints();                        // category: helper: the advice + its glow, if the drawer is open
    bool focus_is_text() const;                  // category: helper: a text box keeps its own Ctrl+Z

    // ── the model ──────────────────────────────────────────────────────────
    // Declaration order matters: the store before the journal that holds it.
    model::GameStore   m_store;
    model::GameJournal m_journal{m_store};
    Engine             m_engine;                 // the strategy table (hints; the computer player)
    sigc::connection   m_progress_tick;          // repaints "Working out the odds… N%"
    std::uint64_t      m_game_serial = 0;        // part of the roll key: a new game's first roll always animates
    model::settings::Settings  m_settings;
    model::stats::History      m_history;        // every finished game (s006)
    bool                       m_recorded = false;   // this game is in the history
    std::string                m_news;           // game over: who won, any new best
    Glib::RefPtr<Gio::SimpleAction> m_act_dice;
    Glib::RefPtr<Gio::SimpleAction> m_act_sound;
    Glib::RefPtr<Gio::SimpleAction> m_act_pace;
    std::vector<model::Player> m_seats;          // the table: who sits where, person or computer
    sigc::connection   m_cpu_tick;               // the pending computer step (one at a time)
    bool               m_repaint_queued = false;

    // ── widgets ────────────────────────────────────────────────────────────
    widgets::Button     m_logo_button;
    widgets::Button     m_new_button;
    widgets::MenuButton m_menu_button;
    widgets::Button     m_sound_button;          // s012: speaker on / muted (M)

    widgets::Box        m_content;               // play area | scorecard
    widgets::Box        m_play;
    widgets::Label      m_status;
    widgets::Stack      m_dice_stack;            // "3d" (DiceView) or "flat" (DiceRow, if GL fails)
    DiceView            m_dice3d;
    widgets::Overlay    m_table;                 // the 3D view + the orb over its corner (s008)
    ViewOrb             m_orb{m_dice3d};
    widgets::Button     m_reset_view;            // back to the standard view (Home)
    // s013: whose turn it is, a card over the table when the turn passes
    // (two or more seats); it fades by itself, and clicks pass through it.
    widgets::Revealer   m_turn_reveal;
    widgets::Box        m_turn_card;
    widgets::Label      m_turn_name;
    widgets::Label      m_turn_info;
    sigc::connection    m_turn_hide;
    // s014: the score in flight from the dice to its box.
    widgets::Label      m_fly;
    guint               m_fly_tick = 0;
    gint64              m_fly_t0 = -1;
    int                 m_fly_player = -1, m_fly_box = -1;
    std::uint64_t       m_turn_seen = ~0ull;      // game / round / seat last announced
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
    std::unique_ptr<NewGameDialog>   m_newgame;
    std::unique_ptr<ShortcutsDialog> m_shortcuts;
};

}  // namespace yatze
