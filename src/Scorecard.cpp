#include "Scorecard.hpp"

#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/separator.h>

#include <string>

namespace yatze {

using model::Category;

namespace {
Gtk::Label* cell(const std::string& text, float xalign = 1.0f) {
    auto* l = Gtk::make_managed<Gtk::Label>(text);
    l->set_xalign(xalign);
    l->set_margin_start(6);
    l->set_margin_end(6);
    l->add_css_class("card-cell");   // one row height whether a box is a button or not
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

void Scorecard::set(const model::GameState& g) {
    clear();
    const int n = int(g.players.size());
    const bool live = !g.over() && g.rolls_used > 0;
    const model::Scoring opt =
        n ? model::score_options(g.player().card, g.dice) : model::Scoring{};

    // Header: player names; the current one marked.
    for (int p = 0; p < n; ++p) {
        auto* h = cell(g.players[p].name, 0.5f);
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
                attach(*cell(std::to_string(card.box[i])), 1 + p, row);
            } else if (p == g.current && live) {
                // What it would score; a button only where the rules allow it.
                auto* b = Gtk::make_managed<Gtk::Button>(opt.allowed[i] ? std::to_string(opt.points[i]) : "");
                b->set_name("play.score." + std::to_string(i));
                b->add_css_class("score-option");
                if (opt.allowed[i] && opt.points[i] == 0) b->add_css_class("zero");
                b->set_sensitive(opt.allowed[i]);
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

}  // namespace yatze
