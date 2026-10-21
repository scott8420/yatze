#include "Shell.hpp"
#include "AboutWindow.hpp"      // complete types for the unique_ptr dtors
#include "NewGameDialog.hpp"
#include "ShortcutsDialog.hpp"
#include "Log.hpp"
#include "model/TableFile.hpp"

// Shell.cpp -- GLUE. The ctor and the ordered build_ui(); every other body
// lives in a category file (index: Shell.hpp).

namespace yatze {

Shell::Shell()
    : m_logo_button("shell.logo_button"),
      m_new_button("shell.new_button"),
      m_menu_button("shell.menu_button"),
      m_content("shell.content", Gtk::Orientation::HORIZONTAL),
      m_play("play", Gtk::Orientation::VERTICAL, 18),
      m_status("play.status"),
      m_dice_stack("play.dice_stack"),
      m_table("play.table"),
      m_reset_view("play.reset_view"),
      m_roll("play.roll"),
      m_said("play.said"),
      m_overlay("drawers.overlay"),
      m_scrim("drawers.scrim", Gtk::Orientation::VERTICAL),
      m_left_tab("drawers.left_tab"),
      m_right_tab("drawers.right_tab"),
      m_left_drawer("drawers.left"),
      m_right_drawer("drawers.right") {
    set_name("shell.window");
}

Shell::~Shell() {   // no tick into a dead window
    m_progress_tick.disconnect();
    m_cpu_tick.disconnect();
}

// Construct, bind, deal the first game, paint.
void Shell::build_ui() {
    build_shell();     // zone (calls build_drawers)
    bind_actions();    // bindings
    m_history.load(history_file());
    m_settings = model::settings::load(settings_file());
    m_act_dice->change_state(Glib::ustring(model::settings::key(m_settings.dice)));
    apply_settings();

    // The players from last time (Scott, s004); the first time ever, you against
    // the computer at Medium. Ctrl+Shift+N changes it.
    m_seats = model::table_file::load(table_file());
    if (m_seats.empty()) m_seats = {{"You"}, {"Computer", model::Seat::Computer, model::Level::Medium}};
    start_game(m_seats);
    m_engine.start();  // cache, or a background build (Engine.hpp)
    if (auto lg = log::get(log::Area::Shell)) lg->info("ready");
}

}  // namespace yatze
