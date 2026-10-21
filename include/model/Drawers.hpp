#pragma once
// model::Drawers -- the EDGE-DRAWER paradigm's state, GTK-free.
//
// The stone: two hidden panels that slide in OVER the content from the left
// and right edges (a GtkRevealer each, floated in a GtkOverlay), so opening
// one never reflows what is under it. A quiet pull tab on each edge, invisible
// until hovered (pure CSS -- see Css.hpp); a transparent scrim catches a click
// outside to close; Esc closes; a key toggles each. In the games the LEFT
// drawer teaches (hints, rules) and the RIGHT reports (stats, scores).
//
// The one rule worth a test: AT MOST ONE OPEN. Opening one closes the other,
// toggling the open one closes it, and the scrim is shown iff one is open.
// That lives here, as plain data, so the selftest guards it with no display;
// the Shell only paints what this says (Shell_helpers.cpp, apply_drawers).
//
// (Distilled from Sudoku's MainWindow twins, themselves Folio's focus drawer.)
namespace yatze::model {

class Drawers {
public:
    enum class Side { None, Left, Right };

    Side open() const { return m_open; }
    bool is_open(Side s) const { return s != Side::None && m_open == s; }
    bool scrim() const { return m_open != Side::None; }   // click-away catcher shown

    // Each returns true when the state changed (the caller repaints only then).
    bool show(Side s)   { return set(s); }
    bool toggle(Side s) { return set(m_open == s ? Side::None : s); }
    bool close()        { return set(Side::None); }

private:
    bool set(Side s) {
        if (s == m_open) return false;
        m_open = s;
        return true;
    }
    Side m_open = Side::None;
};

}  // namespace yatze::model
