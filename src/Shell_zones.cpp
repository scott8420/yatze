#include "Shell.hpp"
#include "A11y.hpp"

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
    a11y::label(m_logo_button, "About Yatz\u00e9!");

    m_new_button.set_label("New Game");
    m_new_button.set_tooltip_text("New game, same players  (Ctrl+N; Ctrl+Shift+N to change them)");
    m_new_button.set_action_name("win.new-game");
    header->pack_start(m_new_button);

    m_menu_button.set_icon_name("open-menu-symbolic");
    m_menu_button.set_menu_model(build_menu());
    header->pack_end(m_menu_button);
    a11y::label(m_menu_button, "Main menu");
    m_sound_button.set_has_frame(false);
    m_sound_button.set_action_name("win.sound");   // the same toggle as M and the menu item
    header->pack_end(m_sound_button);
    set_titlebar(*header);

    // Play area: whose turn, the dice, Roll, what undo just did.
    m_play.set_hexpand(true);
    m_play.set_vexpand(true);
    m_play.set_margin(18);
    m_status.add_css_class("title-3");
    m_status.set_wrap(true);
    m_status.set_justify(Gtk::Justification::CENTER);
    m_play.append(m_status);
    // The dice: the 3D table, or the flat row if OpenGL cannot start.
    // The orb and the reset button float over the table's top-right corner.
    m_table.set_child(m_dice3d);
    auto orb_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    orb_box->set_halign(Gtk::Align::END);
    orb_box->set_valign(Gtk::Align::START);
    orb_box->set_margin(8);
    orb_box->append(m_orb);
    m_reset_view.set_icon_name("view-refresh-symbolic");
    m_reset_view.set_tooltip_text("Standard view (Home)");
    m_reset_view.add_css_class("osd");
    m_reset_view.add_css_class("circular");
    m_reset_view.set_halign(Gtk::Align::CENTER);
    m_reset_view.set_action_name("win.reset-view");
    orb_box->append(m_reset_view);
    a11y::label(m_reset_view, "Standard view");
    a11y::label(m_orb, "View orb");
    a11y::describe(m_orb, "Drag to turn the table; click an axis to look along it");
    m_table.add_overlay(*orb_box);
    m_turn_name.add_css_class("title-1");
    m_turn_info.add_css_class("turn-info");
    m_turn_card.add_css_class("turn-card");
    m_turn_card.append(m_turn_name);
    m_turn_card.append(m_turn_info);
    m_turn_reveal.set_child(m_turn_card);
    m_turn_reveal.set_transition_type(Gtk::RevealerTransitionType::CROSSFADE);
    m_turn_reveal.set_transition_duration(260);
    m_turn_reveal.set_halign(Gtk::Align::CENTER);
    m_turn_reveal.set_valign(Gtk::Align::CENTER);
    m_turn_reveal.set_can_target(false);         // never in the way of a click on a die
    m_table.add_overlay(m_turn_reveal);
    m_dice_stack.add(m_table, "3d");
    m_dice_stack.add(m_dice, "flat");
    m_dice_stack.set_visible_child("3d");
    m_dice_stack.set_vexpand(true);
    m_dice3d.signal_failed().connect([this] { m_dice_stack.set_visible_child("flat"); });
    m_play.append(m_dice_stack);
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

    // s014: the flying score, above the play area and the card.
    m_fly.add_css_class("fly-score");
    m_fly.set_halign(Gtk::Align::START);
    m_fly.set_valign(Gtk::Align::START);
    m_fly.set_can_target(false);
    m_fly.set_visible(false);
    m_overlay.add_overlay(m_fly);
    m_overlay.add_overlay(m_scrim);
    m_overlay.add_overlay(m_left_tab);
    m_overlay.add_overlay(m_right_tab);
    m_overlay.add_overlay(m_left_drawer);
    m_overlay.add_overlay(m_right_drawer);
}

Glib::RefPtr<Gio::Menu> Shell::build_menu() {  // zone: hamburger model
    auto menu = Gio::Menu::create();

    auto game = Gio::Menu::create();
    game->append("New game", "win.new-game");
    game->append("New game\u2026", "win.new-game-dialog");   // who plays
    game->append("Clear high scores\u2026", "win.clear-history");
    menu->append_section(game);

    m_edit_menu = Gio::Menu::create();   // re-labelled live by update_undo_ui
    m_edit_menu->append("Undo", "win.undo");
    m_edit_menu->append("Redo", "win.redo");
    menu->append_section(m_edit_menu);

    auto dice = Gio::Menu::create();   // radio items: the action's state is the choice
    dice->append("Red dice",   "win.dice-style::red");
    dice->append("Ivory dice", "win.dice-style::ivory");
    dice->append("Dice sounds", "win.sound");
    menu->append_section(dice);

    auto pace = Gio::Menu::create();   // s017: radio items, like the dice
    pace->append("Computer: watch every roll", "win.computer-pace::watch");
    pace->append("Computer: quicker", "win.computer-pace::quick");
    menu->append_section(pace);

    auto view = Gio::Menu::create();
    view->append("Rules and hints", "win.toggle-left-drawer");
    view->append("Standings", "win.toggle-right-drawer");
    view->append("Standard view", "win.reset-view");
    menu->append_section(view);

    auto app = Gio::Menu::create();
    app->append("Keyboard shortcuts", "win.shortcuts");
    app->append("About Yatzé!", "win.about");
    app->append("Quit", "win.quit");
    menu->append_section(app);
    return menu;
}

}  // namespace yatze
