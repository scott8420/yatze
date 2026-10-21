#include "Shell.hpp"
#include "AboutWindow.hpp"      // complete types for the unique_ptr dtors
#include "ShortcutsDialog.hpp"
#include "Log.hpp"

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

Shell::~Shell() = default;

// Construct, bind, deal the first game, paint.
void Shell::build_ui() {
    build_shell();     // zone (calls build_drawers)
    bind_actions();    // bindings
    on_new_game(m_table);
    if (auto lg = log::get(log::Area::Shell)) lg->info("ready");
}

}  // namespace yatze
