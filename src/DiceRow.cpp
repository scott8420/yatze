#include "DiceRow.hpp"

#include <cmath>
#include <string>

namespace yatze {

namespace {
// Pip positions on a 3x3 lattice (col, row) in 0..2, per face.
const std::vector<std::pair<int, int>>& pips(int face) {
    static const std::array<std::vector<std::pair<int, int>>, 7> k = {{
        {},
        {{1, 1}},
        {{0, 0}, {2, 2}},
        {{0, 0}, {1, 1}, {2, 2}},
        {{0, 0}, {2, 0}, {0, 2}, {2, 2}},
        {{0, 0}, {2, 0}, {1, 1}, {0, 2}, {2, 2}},
        {{0, 0}, {2, 0}, {0, 1}, {2, 1}, {0, 2}, {2, 2}},
    }};
    return k[face >= 0 && face <= 6 ? face : 0];
}

void rounded_rect(const Cairo::RefPtr<Cairo::Context>& cr, double x, double y, double w, double h, double r) {
    cr->begin_new_sub_path();
    cr->arc(x + w - r, y + r,     r, -M_PI / 2, 0);
    cr->arc(x + w - r, y + h - r, r, 0, M_PI / 2);
    cr->arc(x + r,     y + h - r, r, M_PI / 2, M_PI);
    cr->arc(x + r,     y + r,     r, M_PI, 3 * M_PI / 2);
    cr->close_path();
}
}  // namespace

DiceRow::DiceRow() : widgets::Box("play.dice", Gtk::Orientation::HORIZONTAL, 14) {
    set_halign(Gtk::Align::CENTER);
    for (int i = 0; i < 5; ++i) {
        const std::string n = std::to_string(i + 1);
        auto* face = Gtk::make_managed<widgets::DrawingArea>("play.die" + n + ".face");
        face->set_content_width(72);
        face->set_content_height(72);
        face->set_draw_func([this, i](const Cairo::RefPtr<Cairo::Context>& cr, int w, int h) { draw_die(cr, w, h, i); });
        auto* b = Gtk::make_managed<widgets::Button>("play.die" + n);
        b->set_child(*face);
        b->add_css_class("die");
        b->set_has_frame(false);
        b->set_tooltip_text("Die " + n + " -- click (or press " + n + ") to hold");
        b->signal_clicked().connect([this, i] { m_sig_die.emit(i); });
        append(*b);
        m_buttons[i] = b;
        m_faces[i]   = face;
    }
}

void DiceRow::set(const model::Dice& dice, const std::array<bool, 5>& held, bool can_hold) {
    m_dice = dice;
    m_held = held;
    for (int i = 0; i < 5; ++i) {
        m_buttons[i]->set_sensitive(can_hold);
        if (held[i]) m_buttons[i]->add_css_class("held");
        else         m_buttons[i]->remove_css_class("held");
        m_faces[i]->queue_draw();
    }
}

// A die is a physical thing: ivory with dark pips in light and dark themes
// alike. A held die gets the accent ring and sits a little lower ("set aside").
void DiceRow::draw_die(const Cairo::RefPtr<Cairo::Context>& cr, int w, int h, int i) {
    const double s   = std::min(w, h) - 8.0;
    const double x   = (w - s) / 2.0;
    const double y   = (h - s) / 2.0 + (m_held[i] ? 3.0 : 0.0);
    const double r   = s * 0.16;
    const int    f   = m_dice[i];

    // shadow
    rounded_rect(cr, x + 2, y + 3, s, s, r);
    cr->set_source_rgba(0, 0, 0, 0.25);
    cr->fill();
    // body
    rounded_rect(cr, x, y, s, s, r);
    if (f == 0) cr->set_source_rgba(0.80, 0.80, 0.78, 0.35);   // not rolled yet
    else        cr->set_source_rgb(0.98, 0.97, 0.93);
    cr->fill_preserve();
    if (m_held[i]) { cr->set_source_rgb(0.21, 0.52, 0.89); cr->set_line_width(4.0); }
    else           { cr->set_source_rgba(0, 0, 0, 0.35);      cr->set_line_width(1.0); }
    cr->stroke();
    // pips
    cr->set_source_rgb(0.12, 0.12, 0.14);
    const double step = s / 4.0, pr = s * 0.085;
    for (auto [cx, cy] : pips(f)) {
        cr->arc(x + step * (cx + 1), y + step * (cy + 1), pr, 0, 2 * M_PI);
        cr->fill();
    }
}

}  // namespace yatze
