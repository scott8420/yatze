#include "Shell.hpp"
#include "Log.hpp"

#include <glibmm/variant.h>
#include <gtkmm/eventcontrollerkey.h>

// Shell_bindings.cpp -- BINDINGS. The win.* actions (keys come from the
// shortcut registry, wired in App), the model <-> journal <-> UI wiring, and
// the window-heard keys (Ctrl+Z family, Esc for the drawers).

namespace yatze {

void Shell::bind_actions() {  // bindings
    add_action("new-game", [this] { on_new_game(0); });
    add_action_with_parameter("new-table", Glib::Variant<int>::variant_type(),
        [this](const Glib::VariantBase& v) {
            on_new_game(Glib::VariantBase::cast_dynamic<Glib::Variant<int>>(v).get());
        });
    m_act_roll = add_action("roll", sigc::mem_fun(*this, &Shell::on_roll));
    for (int i = 0; i < 5; ++i)
        add_action("hold-" + std::to_string(i + 1), [this, i] { on_hold(i); });
    m_act_undo = add_action("undo", sigc::mem_fun(*this, &Shell::on_undo));
    m_act_redo = add_action("redo", sigc::mem_fun(*this, &Shell::on_redo));
    add_action("toggle-left-drawer",  sigc::mem_fun(*this, &Shell::on_toggle_left_drawer));
    add_action("toggle-right-drawer", sigc::mem_fun(*this, &Shell::on_toggle_right_drawer));
    add_action("about",     sigc::mem_fun(*this, &Shell::on_about));
    add_action("shortcuts", sigc::mem_fun(*this, &Shell::on_shortcuts));
    add_action("quit",      [this] { close(); });

    // The widgets ask; the handlers act through the verbs.
    m_dice.signal_die().connect(sigc::mem_fun(*this, &Shell::on_hold));
    m_card.signal_pick().connect(sigc::mem_fun(*this, &Shell::on_score));

    // Model <-> journal <-> UI. The store asks the journal whether the door is
    // open (a write outside it counts as a stray -- the selftest's gate); any
    // change repaints once per idle; the journal re-labels Undo / Redo.
    m_store.set_door([this] { return m_journal.open(); });
    m_store.set_on_change([this] { queue_repaint(); });
    m_journal.set_on_change([this] { update_undo_ui(); });

    // ── Keys the WINDOW hears (bubble phase: only what nothing else took) ────
    // Ctrl+Z is not an accelerator, so a text box (none yet; the New Game
    // dialog will have names) keeps its own. Esc closes a drawer, and only
    // while one is open. (Both from Cairn.)
    auto keys = Gtk::EventControllerKey::create();
    keys->signal_key_pressed().connect(
        [this](guint keyval, guint, Gdk::ModifierType state) {
            const auto mods = state & (Gdk::ModifierType::CONTROL_MASK |
                                       Gdk::ModifierType::SHIFT_MASK |
                                       Gdk::ModifierType::ALT_MASK);
            const auto CTRL  = Gdk::ModifierType::CONTROL_MASK;
            const auto SHIFT = Gdk::ModifierType::SHIFT_MASK;
            if (keyval == GDK_KEY_Escape && mods == Gdk::ModifierType{} && m_drawers.scrim()) {
                on_close_drawers();
                return true;
            }
            const char* act = nullptr;
            if ((keyval == GDK_KEY_z || keyval == GDK_KEY_Z) && mods == CTRL)               act = "win.undo";
            else if ((keyval == GDK_KEY_z || keyval == GDK_KEY_Z) && mods == (CTRL | SHIFT)) act = "win.redo";
            else if ((keyval == GDK_KEY_y || keyval == GDK_KEY_Y) && mods == CTRL)          act = "win.redo";
            if (!act || focus_is_text()) return false;
            activate_action(act);
            return true;
        }, false);
    add_controller(keys);
}

}  // namespace yatze
