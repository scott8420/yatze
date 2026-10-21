#include "ViewOrb.hpp"
#include "DiceView.hpp"
#include "gfx/Orbit.hpp"

#include <gtkmm/eventcontrollermotion.h>
#include <gtkmm/gesturedrag.h>

#include <algorithm>
#include <cmath>

namespace yatze {

namespace {
struct Rgb { double r, g, b; };
const Rgb kAxis[3] = {{0.90, 0.29, 0.30}, {0.33, 0.70, 0.36}, {0.29, 0.52, 0.98}};   // X, Y, Z
const char* kName[3] = {"X", "Y", "Z"};
constexpr int kSize = 92;
}  // namespace

ViewOrb::ViewOrb(DiceView& view) : widgets::DrawingArea("play.orb"), m_view(view) {
    set_content_width(kSize);
    set_content_height(kSize);
    set_halign(Gtk::Align::END);
    set_valign(Gtk::Align::START);
    set_cursor("grab");
    set_tooltip_text("Drag to turn the table · click an axis to look along it");
    set_draw_func(sigc::mem_fun(*this, &ViewOrb::draw));
    m_view.signal_frame().connect([this] { queue_draw(); });

    // Drag: orbit. A press that hardly moves is a click (below).
    auto drag = Gtk::GestureDrag::create();
    drag->signal_drag_begin().connect([this](double x, double y) {
        m_press_x = x;
        m_press_y = y;
        m_last_x = m_last_y = 0;
        m_moved = 0;
        set_cursor("grabbing");
    });
    drag->signal_drag_update().connect([this](double ox, double oy) {
        const double dx = ox - m_last_x, dy = oy - m_last_y;
        m_last_x = ox;
        m_last_y = oy;
        m_moved += std::fabs(dx) + std::fabs(dy);
        if (m_moved > 3) m_view.orbit_by(float(dx), float(dy));
    });
    // Release: a press that hardly moved is a CLICK -- on a ball, look along
    // that axis (the nearest ball wins). One gesture does both: a separate
    // click gesture never saw the press once the drag had claimed it (s008).
    drag->signal_drag_end().connect([this](double, double) {
        set_cursor("grab");
        if (m_moved > 3) return;
        const auto bs = balls(get_width(), get_height());
        int best = -1;
        float depth = 9;
        for (const auto& b : bs)
            if (std::hypot(m_press_x - b.x, m_press_y - b.y) <= b.r + 3 && b.depth < depth) { depth = b.depth; best = b.axis; }
        if (best >= 0) m_view.snap_to(best);
    });
    add_controller(drag);

    auto motion = Gtk::EventControllerMotion::create();
    motion->signal_enter().connect([this](double, double) { m_hover = true; queue_draw(); });
    motion->signal_leave().connect([this] { m_hover = false; queue_draw(); });
    add_controller(motion);
}

// Where each axis ball sits on the orb, from the camera the table uses now.
std::vector<ViewOrb::Ball> ViewOrb::balls(int w, int h) const {
    const double cx = w / 2.0, cy = h / 2.0, reach = std::min(w, h) / 2.0 - 13;
    std::vector<Ball> out;
    for (int i = 0; i < 6; ++i) {
        const auto p = gfx::project_axis(gfx::axis_vec(i), m_view.cam_eye(), m_view.cam_at());
        const bool pos = i % 2 == 0;
        out.push_back({i, cx + p.x * reach, cy - p.y * reach, pos ? 9.0 : 6.0, p.depth});
    }
    // Far ones first, so nearer balls draw over them.
    std::sort(out.begin(), out.end(), [](const Ball& a, const Ball& b) { return a.depth > b.depth; });
    return out;
}

void ViewOrb::draw(const Cairo::RefPtr<Cairo::Context>& cr, int w, int h) {
    const double cx = w / 2.0, cy = h / 2.0, rad = std::min(w, h) / 2.0 - 1;
    // The orb: a soft disc, brighter under the pointer.
    cr->arc(cx, cy, rad, 0, 2 * M_PI);
    cr->set_source_rgba(1, 1, 1, m_hover ? 0.20 : 0.10);
    cr->fill_preserve();
    cr->set_source_rgba(1, 1, 1, m_hover ? 0.35 : 0.18);
    cr->set_line_width(1);
    cr->stroke();

    for (const auto& b : balls(w, h)) {
        const auto& c = kAxis[b.axis / 2];
        const bool pos = b.axis % 2 == 0;
        const double fade = b.depth > 0 ? 0.55 : 1.0;    // behind the orb: dimmer
        if (pos) {   // the positive axis has its stem
            cr->move_to(cx, cy);
            cr->line_to(b.x, b.y);
            cr->set_source_rgba(c.r, c.g, c.b, 0.9 * fade);
            cr->set_line_width(2.2);
            cr->stroke();
        }
        cr->arc(b.x, b.y, b.r, 0, 2 * M_PI);
        if (pos) {
            cr->set_source_rgba(c.r, c.g, c.b, fade);
            cr->fill();
            cr->select_font_face("Sans", Cairo::ToyFontFace::Slant::NORMAL, Cairo::ToyFontFace::Weight::BOLD);
            cr->set_font_size(11);
            Cairo::TextExtents te;
            cr->get_text_extents(kName[b.axis / 2], te);
            cr->move_to(b.x - te.width / 2 - te.x_bearing, b.y - te.height / 2 - te.y_bearing);
            cr->set_source_rgba(0.08, 0.08, 0.1, fade);
            cr->show_text(kName[b.axis / 2]);
        } else {
            cr->set_source_rgba(c.r, c.g, c.b, 0.30 * fade);
            cr->fill_preserve();
            cr->set_source_rgba(c.r, c.g, c.b, 0.85 * fade);
            cr->set_line_width(1.5);
            cr->stroke();
        }
    }
}

}  // namespace yatze
