#include "StatsPanel.hpp"

#include <glibmm/markup.h>
#include <gtkmm/label.h>

#include <algorithm>
#include <numeric>
#include <string>

namespace yatze {

StatsPanel::StatsPanel()
    : widgets::Box("drawer.stats", Gtk::Orientation::VERTICAL, 8),
      m_body("drawer.stats.body") {
    add_css_class("background");
    add_css_class("drawer-panel");
    add_css_class("right");
    set_size_request(260, -1);

    auto* title = Gtk::make_managed<Gtk::Label>("Standings");
    title->set_xalign(0.0f);
    title->add_css_class("title-4");
    append(*title);

    m_body.set_wrap(true);
    m_body.set_xalign(0.0f);
    m_body.set_yalign(0.0f);
    m_body.set_vexpand(true);
    append(m_body);
}

void StatsPanel::show(const model::GameState& g) {
    std::vector<int> order(g.players.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return g.players[a].card.total() > g.players[b].card.total();
    });
    std::string m = g.over() ? "<b>Final</b>\n\n" : "Round " + std::to_string(g.round) + " of 13\n\n";
    for (int i : order) {
        const auto& p = g.players[i];
        m += Glib::Markup::escape_text(p.name) + ":  <b>" + std::to_string(p.card.total()) + "</b>\n";
    }
    m += "\n<i>High scores and history arrive in a later milestone.</i>";
    m_body.set_markup(m);
}

}  // namespace yatze
