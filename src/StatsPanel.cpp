#include "StatsPanel.hpp"

#include <glibmm/markup.h>
#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>

#include <algorithm>
#include <cstdio>
#include <numeric>
#include <string>

namespace yatze {

namespace {
std::string esc(const std::string& s) { return Glib::Markup::escape_text(s); }
std::string num1(double v) { char b[32]; std::snprintf(b, sizeof b, "%.1f", v); return b; }
}  // namespace

StatsPanel::StatsPanel()
    : widgets::Box("drawer.stats", Gtk::Orientation::VERTICAL, 8),
      m_body("drawer.stats.body") {
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
    scroll->set_child(m_body);
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
}

}  // namespace yatze
