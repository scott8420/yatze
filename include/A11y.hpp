#pragma once
// A11y -- what a screen reader hears (s018). Thin wrappers over GTK's C
// accessibility calls (gtkmm 4.10 wraps only part of them).
//   label(w, text)     the name read for a widget (icon-only buttons, the
//                      3D table, the orb)
//   announce(w, text)  a polite spoken note: a roll landed, a box taken,
//                      whose turn it is. GTK 4.14+; older GTK says nothing.
#include <gtk/gtk.h>
#include <gtkmm/widget.h>

#include <string>

namespace yatze::a11y {

inline void label(Gtk::Widget& w, const std::string& text) {
    gtk_accessible_update_property(GTK_ACCESSIBLE(w.gobj()), GTK_ACCESSIBLE_PROPERTY_LABEL, text.c_str(), -1);
}

inline void describe(Gtk::Widget& w, const std::string& text) {
    gtk_accessible_update_property(GTK_ACCESSIBLE(w.gobj()), GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, text.c_str(), -1);
}

inline void announce([[maybe_unused]] Gtk::Widget& w, [[maybe_unused]] const std::string& text) {
#if GTK_CHECK_VERSION(4, 14, 0)
    // Only to a real screen-reader bridge. Without an accessibility bus GTK
    // falls back to a context that cannot announce, and GTK 4.14 then calls a
    // NULL function -- the app crashed (s018, found in the sandbox).
    GtkATContext* ctx = gtk_accessible_get_at_context(GTK_ACCESSIBLE(w.gobj()));
    if (!ctx) return;
    const bool atspi = std::string(G_OBJECT_TYPE_NAME(ctx)) == "GtkAtSpiContext";
    g_object_unref(ctx);
    if (atspi) gtk_accessible_announce(GTK_ACCESSIBLE(w.gobj()), text.c_str(), GTK_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_MEDIUM);
#endif
}

}  // namespace yatze::a11y
