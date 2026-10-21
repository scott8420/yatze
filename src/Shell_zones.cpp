#include "Shell.hpp"

#include <gtkmm/gestureclick.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/separator.h>

// Shell_zones.cpp -- ZONES. The header, the play area beside the scorecard,
// the drawers over both, the menu model.

namespace yatze {

void Shell::build_shell() {  // zone: header + play area + scorecard
    set_title("Yatzé!");
    set_default_size(920, 760);   // the whole card, Total included, without scrolling

    auto header = Gtk::make_managed<Gtk::HeaderBar>();
    m_logo_button.set_icon_name("yatze-logo-symbolic");
    m_logo_button.set_has_frame(false);
    m_logo_button.set_tooltip_text("About Yatzé!");
    m_logo_button.set_action_name("win.about");
    header->pack_start(m_logo_button);

    m_new_button.set_label("New Game");
    m_new_button.set_tooltip_text("New game, same table  (Ctrl+N)");
    m_new_button.set_action_name("win.new-game");
    header->pack_start(m_new_button);

    m_menu_button.set_icon_name("open-menu-symbolic");
    m_menu_button.set_menu_model(build_menu());
    header->pack_end(m_menu_button);
    set_titlebar(*header);

    // Play area: whose turn, the dice, Roll, what undo just did.
    m_play.set_hexpand(true);
    m_play.set_valign(Gtk::Align::CENTER);
    m_play.set_margin(24);
    m_status.add_css_class("title-3");
    m_status.set_wrap(true);
    m_status.set_justify(Gtk::Justification::CENTER);
    m_play.append(m_status);
    m_play.append(m_dice);
    m_roll.set_label("Roll");
    m_roll.add_css_class("suggested-action");
    m_roll.add_css_class("pill");
    m_roll.set_halign(Gtk::Align::CENTER);
    m_roll.set_size_request(160, 44);
    m_roll.set_action_name("win.roll");
    m_play.append(m_roll);
    m_said.add_css_class("dim-label");
    m_said.set_halign(Gtk::Align::CENTER);
    m_play.append(m_said);

    // The scorecard: part of the main view (Scott), scrolls if the window is short.
    auto scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    scroll->set_propagate_natural_width(true);
    scroll->set_child(m_card);

    m_content.append(m_play);
    m_content.append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::VERTICAL));
    m_content.append(*scroll);

    build_drawers();
    set_child(m_overlay);
}

// The edge drawers (Cairn's stone): the content is the overlay's BASE and the
// drawer parts float on top, so opening one never reflows the card or the dice.
// Left = rules / hints, right = standings. Order bottom -> top: scrim, tabs,
// revealers. One open at a time: model::Drawers decides, apply_drawers paints.
void Shell::build_drawers() {  // zone: overlay, two drawers, tabs, scrim
    m_overlay.set_child(m_content);

    m_left_drawer.set_child(m_hints);
    m_left_drawer.set_transition_type(Gtk::RevealerTransitionType::SLIDE_RIGHT);
    m_left_drawer.set_transition_duration(240);
    m_left_drawer.set_halign(Gtk::Align::START);
    m_left_drawer.set_valign(Gtk::Align::FILL);
    m_left_drawer.set_reveal_child(false);

    m_right_drawer.set_child(m_stats);
    m_right_drawer.set_transition_type(Gtk::RevealerTransitionType::SLIDE_LEFT);
    m_right_drawer.set_transition_duration(240);
    m_right_drawer.set_halign(Gtk::Align::END);
    m_right_drawer.set_valign(Gtk::Align::FILL);
    m_right_drawer.set_reveal_child(false);

    m_scrim.add_css_class("drawer-scrim");
    m_scrim.set_hexpand(true);
    m_scrim.set_vexpand(true);
    m_scrim.set_visible(false);
    {
        auto click = Gtk::GestureClick::create();
        click->signal_released().connect([this](int, double, double) { on_close_drawers(); });
        m_scrim.add_controller(click);
    }

    m_left_tab.set_label("›");    // ›
    m_left_tab.add_css_class("drawer-tab");
    m_left_tab.set_halign(Gtk::Align::START);
    m_left_tab.set_valign(Gtk::Align::CENTER);
    m_left_tab.set_tooltip_text("Rules and hints (F9)");
    m_left_tab.set_action_name("win.toggle-left-drawer");

    m_right_tab.set_label("‹");   // ‹
    m_right_tab.add_css_class("drawer-tab-right");
    m_right_tab.set_halign(Gtk::Align::END);
    m_right_tab.set_valign(Gtk::Align::CENTER);
    m_right_tab.set_tooltip_text("Standings (Shift+F9)");
    m_right_tab.set_action_name("win.toggle-right-drawer");

    m_overlay.add_overlay(m_scrim);
    m_overlay.add_overlay(m_left_tab);
    m_overlay.add_overlay(m_right_tab);
    m_overlay.add_overlay(m_left_drawer);
    m_overlay.add_overlay(m_right_drawer);
}

Glib::RefPtr<Gio::Menu> Shell::build_menu() {  // zone: hamburger model
    auto menu = Gio::Menu::create();

    // Hot seat for now; computer players arrive with the New Game dialog.
    auto game = Gio::Menu::create();
    game->append("New game", "win.new-game");
    auto table = Gio::Menu::create();
    table->append("1 player",  "win.new-table(1)");
    table->append("2 players", "win.new-table(2)");
    table->append("3 players", "win.new-table(3)");
    table->append("4 players", "win.new-table(4)");
    game->append_submenu("New game with…", table);
    menu->append_section(game);

    m_edit_menu = Gio::Menu::create();   // re-labelled live by update_undo_ui
    m_edit_menu->append("Undo", "win.undo");
    m_edit_menu->append("Redo", "win.redo");
    menu->append_section(m_edit_menu);

    auto view = Gio::Menu::create();
    view->append("Rules and hints", "win.toggle-left-drawer");
    view->append("Standings", "win.toggle-right-drawer");
    menu->append_section(view);

    auto app = Gio::Menu::create();
    app->append("Keyboard shortcuts", "win.shortcuts");
    app->append("About Yatzé!", "win.about");
    app->append("Quit", "win.quit");
    menu->append_section(app);
    return menu;
}

}  // namespace yatze
