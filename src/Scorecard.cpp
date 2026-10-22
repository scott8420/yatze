#include "Scorecard.hpp"

#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/separator.h>
#include <glibmm/main.h>

#include <string>

namespace yatze {

using model::Category;

namespace {
// Every cell is this tall (a score button's natural height), so the card never
// changes shape between "before the roll", "after it" and "not your turn".
constexpr int kRowHeight = 30;

Gtk::Label* cell(const std::string& text, float xalign = 1.0f) {
    auto* l = Gtk::make_managed<Gtk::Label>(text);
    l->set_xalign(xalign);
    l->set_margin_start(6);
    l->set_margin_end(6);
    l->set_size_request(-1, kRowHeight);   // one row height, button or not
    return l;
}
}  // namespace

Scorecard::Scorecard() : widgets::Grid("play.scorecard") {
    set_row_spacing(2);
    set_column_spacing(4);
    set_margin(12);
    set_valign(Gtk::Align::START);
}

void Scorecard::clear() {
    while (auto* c = get_first_child()) remove(*c);
}

void Scorecard::set(const model::GameState& g, bool interactive, bool reveal) {
    clear();
    const int n = int(g.players.size());
    const bool live = !g.over() && g.rolls_used > 0 && reveal;
    const model::Scoring opt =
        n ? model::score_options(g.player().card, g.dice) : model::Scoring{};

    // Header: player names; the current one marked.
    for (int p = 0; p < n; ++p) {
        const auto& pl = g.players[p];
        auto* h = cell(pl.name, 0.5f);
        if (pl.seat == model::Seat::Computer)
            h->set_tooltip_text(std::string("Computer \u2014 ") +
                                (pl.level == model::Level::Easy ? "Easy" : pl.level == model::Level::Medium ? "Medium" : "Hard"));
        h->add_css_class("heading");
        if (p == g.current && !g.over()) h->add_css_class("current-player");
        attach(*h, 1 + p, 0);
    }

    int row = 1;
    const auto box_row = [&](Category c) {
        auto* name = cell(std::string(model::name(c)), 0.0f);
        name->set_tooltip_text(std::string(model::rule_text(c)));
        attach(*name, 0, row);
        for (int p = 0; p < n; ++p) {
            const auto& card = g.players[p].card;
            const int i = model::index(c);
            if (!card.open(c)) {
                auto* v = cell(std::to_string(card.box[i]));
                v->add_css_class("score-cell");
                if (p == m_flash_player && i == m_flash_box) v->add_css_class("just-scored");
                attach(*v, 1 + p, row);
            } else if (p == g.current && live) {
                // What it would score; a button only where the rules allow it.
                auto* b = Gtk::make_managed<Gtk::Button>(opt.allowed[i] ? std::to_string(opt.points[i]) : "");
                b->set_name("play.score." + std::to_string(i));
                b->add_css_class("score-option");
                b->set_size_request(-1, kRowHeight);
                if (i == m_suggested && interactive && opt.allowed[i]) b->add_css_class("suggested");
                if (opt.allowed[i] && opt.points[i] == 0) b->add_css_class("zero");
                b->set_sensitive(interactive && opt.allowed[i]);
                if (opt.allowed[i])
                    b->set_tooltip_text("Take " + std::to_string(opt.points[i]) + " in " + std::string(model::name(c)));
                b->signal_clicked().connect([this, c] { m_sig_pick.emit(c); });
                attach(*b, 1 + p, row);
            } else {
                attach(*cell(""), 1 + p, row);
            }
        }
        ++row;
    };
    const auto sum_row = [&](const std::string& label, auto value_of, bool strong) {
        auto* l = cell(label, 0.0f);
        if (strong) l->add_css_class("heading"); else l->add_css_class("dim-label");
        attach(*l, 0, row);
        for (int p = 0; p < n; ++p) {
            auto* v = cell(value_of(g.players[p].card));
            if (strong) v->add_css_class("heading"); else v->add_css_class("dim-label");
            attach(*v, 1 + p, row);
        }
        ++row;
    };
    const auto rule = [&] {
        auto* s = Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL);
        s->set_margin_top(3);
        s->set_margin_bottom(3);
        attach(*s, 0, row++, 1 + n, 1);
    };

    for (int i = 0; i < model::kUpper; ++i) box_row(model::category(i));
    rule();
    sum_row("Upper", [](const model::Card& c) { return std::to_string(c.upper_sum()); }, false);
    sum_row("Bonus (63+)", [](const model::Card& c) {
        if (c.upper_bonus()) return std::string("35");
        bool all = true;
        for (int i = 0; i < model::kUpper; ++i) if (c.box[i] == model::kOpen) all = false;
        return all ? std::string("0") : std::to_string(model::kBonusAt - c.upper_sum()) + " to go";
    }, false);
    rule();
    for (int i = model::kUpper; i < model::kCategories; ++i) box_row(model::category(i));
    rule();
    sum_row("Yatzé bonus", [](const model::Card& c) {
        return c.bonuses ? "+" + std::to_string(c.bonuses * model::kYatzeBonus) : std::string("");
    }, false);
    sum_row("Total", [](const model::Card& c) { return std::to_string(c.total()); }, true);
}

// Rows: header 0; upper boxes 1..6; rule, Upper, Bonus, rule (7..10); lower
// boxes from 11.
Gtk::Widget* Scorecard::box_cell(int player, int box) {
    if (box < 0 || box >= model::kCategories) return nullptr;
    const int row = box < model::kUpper ? 1 + box : 11 + (box - model::kUpper);
    return get_child_at(1 + player, row);
}

void Scorecard::flash(int player, int box) {
    m_flash_player = player;
    m_flash_box = box;
    if (auto* w = box_cell(player, box)) w->add_css_class("just-scored");
    m_flash_off.disconnect();
    m_flash_off = Glib::signal_timeout().connect([this] {
        if (auto* w = box_cell(m_flash_player, m_flash_box)) w->remove_css_class("just-scored");   // fades (CSS)
        m_flash_player = m_flash_box = -1;
        return false;
    }, 900);
}

void Scorecard::set_suggested(int box) {
    if (box == m_suggested) return;
    m_suggested = box;
    // Re-mark the buttons in place (a full rebuild waits for the next set()).
    for (auto* c = get_first_child(); c; c = c->get_next_sibling()) {
        if (!c->has_css_class("score-option")) continue;
        const std::string n = c->get_name();
        const int i = n.rfind("play.score.", 0) == 0 ? std::stoi(n.substr(11)) : -1;
        if (i == box && c->get_sensitive()) c->add_css_class("suggested");
        else c->remove_css_class("suggested");
    }
}

}  // namespace yatze
