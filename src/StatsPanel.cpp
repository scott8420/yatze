#include "StatsPanel.hpp"

#include <glibmm/markup.h>
#include <gtkmm/label.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/scrolledwindow.h>
#include <cairomm/context.h>

#include <algorithm>
#include <cstdio>
#include <numeric>
#include <string>

namespace yatze {

namespace {
std::string esc(const std::string& s) { return Glib::Markup::escape_text(s); }
std::string num1(double v) { char b[32]; std::snprintf(b, sizeof b, "%.1f", v); return b; }

// The last kShown totals, oldest at the left: a line with a dot per game, the
// whole record's average dashed, the last ten games' average in amber over
// those games, the best ringed. Colours follow the theme's text colour.
class TrendChart : public Gtk::DrawingArea {
public:
    static constexpr int kShown = 40;
    explicit TrendChart(const model::stats::Trend& t) : m_t(t) {
        set_content_height(78);
        set_hexpand(true);
        set_draw_func(sigc::mem_fun(*this, &TrendChart::draw));
    }
private:
    void draw(const Cairo::RefPtr<Cairo::Context>& cr, int w, int h) {
        const auto& all = m_t.totals;
        const int n = std::min<int>(int(all.size()), kShown), first = int(all.size()) - n;
        if (n < 2) return;
        const Gdk::RGBA fg = get_color();
        int lo = 1000, hi = 0;
        for (int i = first; i < int(all.size()); ++i) { lo = std::min(lo, all[std::size_t(i)]); hi = std::max(hi, all[std::size_t(i)]); }
        lo = std::min(lo, int(m_t.average)) - 10;
        hi = std::max(hi, int(m_t.average)) + 10;
        const double pad = 6;
        auto X = [&](int i) { return pad + (w - 2 * pad) * double(i) / double(n - 1); };
        auto Y = [&](double v) { return h - pad - (h - 2 * pad) * (v - lo) / double(hi - lo); };
        // The whole record's average, dashed.
        cr->set_source_rgba(fg.get_red(), fg.get_green(), fg.get_blue(), 0.35);
        cr->set_line_width(1);
        cr->set_dash(std::vector<double>{4, 3}, 0);
        cr->move_to(pad, Y(m_t.average));
        cr->line_to(w - pad, Y(m_t.average));
        cr->stroke();
        cr->unset_dash();
        // The last ten's average, amber, over those games.
        const int r = std::min(n, model::stats::Trend::kRecent);
        cr->set_source_rgba(1.0, 0.70, 0.22, 0.95);
        cr->set_line_width(2.5);
        cr->move_to(X(n - r), Y(m_t.recent));
        cr->line_to(X(n - 1), Y(m_t.recent));
        cr->stroke();
        // The games.
        cr->set_source_rgba(fg.get_red(), fg.get_green(), fg.get_blue(), 0.8);
        cr->set_line_width(1.5);
        for (int i = 0; i < n; ++i) {
            const double x = X(i), y = Y(all[std::size_t(first + i)]);
            if (i == 0) cr->move_to(x, y); else cr->line_to(x, y);
        }
        cr->stroke();
        for (int i = 0; i < n; ++i) {
            const int v = all[std::size_t(first + i)];
            cr->arc(X(i), Y(v), v == m_t.best ? 3.5 : 2.0, 0, 2 * 3.14159265);
            cr->fill();
        }
    }
    model::stats::Trend m_t;
};
}  // namespace

StatsPanel::StatsPanel()
    : widgets::Box("drawer.stats", Gtk::Orientation::VERTICAL, 8),
      m_body("drawer.stats.body"),
      m_page("drawer.stats.page", Gtk::Orientation::VERTICAL, 6),
      m_trends("drawer.stats.trends", Gtk::Orientation::VERTICAL, 4) {
    add_css_class("background");
    add_css_class("drawer-panel");
    add_css_class("right");
    set_size_request(300, -1);

    m_body.set_wrap(true);
    m_body.set_xalign(0.0f);
    m_body.set_yalign(0.0f);
    m_body.set_selectable(false);
    auto* scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    scroll->set_vexpand(true);
    m_page.append(m_body);
    m_page.append(m_trends);
    scroll->set_child(m_page);
    append(*scroll);
}

void StatsPanel::show(const model::GameState& g, const model::stats::History& h, const std::string& news) {
    std::string m;
    if (!news.empty()) m += "<span size='large'><b>" + esc(news) + "</b></span>\n\n";

    // This game.
    m += "<span size='large'><b>" + std::string(g.over() ? "Final" : "This game") + "</b></span>";
    if (!g.over()) m += "  <span alpha='70%'>round " + std::to_string(g.round) + " of 13</span>";
    m += "\n";
    std::vector<int> order(g.players.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return g.players[a].card.total() > g.players[b].card.total();
    });
    for (int i : order)
        m += "  " + esc(g.players[i].name) + "  <b>" + std::to_string(g.players[i].card.total()) + "</b>\n";

    // High scores.
    const auto top = h.top(10);
    m += "\n<span size='large'><b>High scores</b></span>\n";
    if (top.empty()) m += "  <span alpha='70%'>No games finished yet.</span>\n";
    for (std::size_t i = 0; i < top.size(); ++i)
        m += "  " + std::to_string(i + 1) + ".  <b>" + std::to_string(top[i].total) + "</b>  " + esc(top[i].who) +
             "  <span alpha='60%'>" + esc(top[i].when) + "</span>\n";

    // Players.
    const auto ps = h.players();
    if (!ps.empty()) {
        m += "\n<span size='large'><b>Players</b></span>\n";
        for (const auto& p : ps) {
            m += "  <b>" + esc(p.who) + "</b>\n";
            m += "    <span alpha='75%'>" + std::to_string(p.games) + (p.games == 1 ? " game" : " games") +
                 ", " + std::to_string(p.wins) + (p.wins == 1 ? " win" : " wins") +
                 " · best " + std::to_string(p.best) + " · average " + num1(p.average) +
                 (p.yatzes ? " · " + std::to_string(p.yatzes) + " Yatzé" + (p.yatzes == 1 ? "" : "s") : "") +
                 "</span>\n";
        }
    }
    m_body.set_markup(m);

    // Over time (s015): people with two games or more.
    while (auto* c = m_trends.get_first_child()) m_trends.remove(*c);
    bool head = false;
    for (const auto& p : ps) {
        if (p.computer || p.games < 2) continue;
        const auto t = h.trend(p.who);
        if (!head) {
            auto* l = Gtk::make_managed<Gtk::Label>();
            l->set_markup("<span size='large'><b>Over time</b></span>");
            l->set_xalign(0.0f);
            l->set_margin_top(6);
            m_trends.append(*l);
            head = true;
        }
        const double diff = t.recent - t.average;
        std::string line = "<b>" + esc(p.who) + "</b>  <span alpha='75%'>last " +
                           std::to_string(std::min(t.games, model::stats::Trend::kRecent)) + ": <b>" + num1(t.recent) +
                           "</b> (" + (diff >= 0 ? "+" : "") + num1(diff) + " vs. average) · bonus " +
                           std::to_string(int(t.bonus_rate * 100 + 0.5)) + "%</span>";
        auto* l = Gtk::make_managed<Gtk::Label>();
        l->set_markup(line);
        l->set_xalign(0.0f);
        l->set_wrap(true);
        m_trends.append(*l);
        m_trends.append(*Gtk::make_managed<TrendChart>(t));
    }
}

}  // namespace yatze
