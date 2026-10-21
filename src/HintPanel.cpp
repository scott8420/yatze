#include "HintPanel.hpp"
#include "Engine.hpp"
#include "model/Rules.hpp"
#include "model/Strategy.hpp"

#include <glibmm/markup.h>
#include <gtkmm/separator.h>

#include <cmath>
#include <cstdio>

#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>

#include <string>

namespace yatze {

namespace {
Gtk::Label* para(const std::string& markup, bool dim = false) {
    auto* l = Gtk::make_managed<Gtk::Label>();
    l->set_markup(markup);
    l->set_wrap(true);
    l->set_xalign(0.0f);
    l->set_max_width_chars(34);
    if (dim) l->add_css_class("dim-label");
    return l;
}
}  // namespace

HintPanel::HintPanel()
    : widgets::Box("drawer.hints", Gtk::Orientation::VERTICAL, 8),
      m_advice("drawer.hints.advice") {
    add_css_class("background");
    add_css_class("drawer-panel");
    add_css_class("left");
    set_size_request(300, -1);

    auto* title = Gtk::make_managed<Gtk::Label>("This roll");
    title->set_xalign(0.0f);
    title->add_css_class("title-4");
    append(*title);

    m_advice.set_wrap(true);
    m_advice.set_xalign(0.0f);
    m_advice.set_max_width_chars(34);
    m_advice.add_css_class("hint-advice");
    append(m_advice);

    auto* body = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
    auto* rules = Gtk::make_managed<Gtk::Label>("Rules");
    rules->set_xalign(0.0f);
    rules->add_css_class("title-4");
    body->append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL));
    body->append(*rules);
    body->append(*para(
        "<b>A turn:</b> roll up to three times. Between rolls, click dice to hold "
        "them. Then take one box on your card -- you must take one, even for 0."));
    body->append(*para("<b>Upper section</b>"));
    for (int i = 0; i < model::kUpper; ++i)
        body->append(*para("  " + std::string(model::name(model::category(i))) + " -- " +
                           std::string(model::rule_text(model::category(i))), true));
    body->append(*para("Upper total of 63 or more: <b>+35</b>. (Three of each face makes 63.)"));
    body->append(*para("<b>Lower section</b>"));
    for (int i = model::kUpper; i < model::kCategories; ++i)
        body->append(*para("  " + std::string(model::name(model::category(i))) + " -- " +
                           std::string(model::rule_text(model::category(i))), true));
    body->append(*para(
        "<b>More Yatzés:</b> each one after a 50 earns <b>+100</b>. It must go in "
        "its own upper box if that is open; otherwise any open lower box, where Full "
        "house and the straights score in full; otherwise any open upper box for 0."));
    body->append(*para(
        "<b>Undo</b> (Ctrl+Z) takes back a hold or the box you just took -- "
        "never a roll.", true));
    body->append(*para(
        "<b>About the advice:</b> it is the best play for <i>your</i> card -- the "
        "highest final score on average, worked out exactly for every roll. The "
        "number beside each choice is that average final score.", true));

    auto* scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    scroll->set_vexpand(true);
    scroll->set_child(*body);
    append(*scroll);
}

namespace {
std::string pips(const model::Dice& kept, int count) {
    static const char* glyph[] = {"", "\u2680", "\u2681", "\u2682", "\u2683", "\u2684", "\u2685"};
    std::string s;
    for (int i = 0; i < count; ++i) { if (i) s += " "; s += glyph[kept[i]]; }
    return "<span size='x-large' rise='-2000'>" + s + "</span>";
}
std::string num(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%.1f", v);
    return b;
}
}  // namespace

void HintPanel::show(const model::GameState& g, const Engine& engine, bool rolling) {
    namespace st = model::strategy;
    if (g.players.empty()) { m_advice.set_text(""); return; }
    if (rolling) { m_advice.set_markup("<i>The dice are rolling\u2026</i>"); return; }
    if (!engine.ready()) {
        m_advice.set_markup("<i>Working out the odds\u2026 " +
                            std::to_string(int(std::lround(engine.progress() * 100))) + "%</i>\n"
                            "(once per computer, a few seconds)");
        return;
    }
    if (g.over()) { m_advice.set_text("The game is over."); return; }

    const auto& card = g.player().card;
    const std::string who = g.players.size() > 1 ? Glib::Markup::escape_text(g.player().name) + ": " : "";
    if (g.rolls_used == 0) {
        const double expect = card.total() + engine.table().future(st::state_of(card));
        m_advice.set_markup(who + "Roll the dice.\n\n<span alpha='70%'>Your card is worth about <b>" +
                            num(expect) + "</b> with best play from here.</span>");
        return;
    }

    const auto a = st::advise(engine.table(), card, g.dice, g.rolls_left());
    if (a.boxes.empty()) { m_advice.set_text(""); return; }
    std::string m = who;
    if (a.roll_on) {
        const auto& h = a.holds.front();
        m += h.count ? "<b>Keep " + pips(h.kept, h.count) + " and roll.</b>" : "<b>Roll all five.</b>";
    } else {
        const auto& b = a.boxes.front();
        m += "<b>Take " + std::string(model::name(b.c)) + " for " + std::to_string(b.points) + ".</b>";
    }
    if (!a.holds.empty()) {
        m += "\n\n<span alpha='70%'>Keep and roll</span>";
        for (std::size_t i = 0; i < a.holds.size() && i < 3; ++i) {
            const auto& h = a.holds[i];
            m += "\n  " + (h.count ? pips(h.kept, h.count) : std::string("nothing")) +
                 "  <span alpha='70%'>\u2192 " + num(h.expect) + "</span>";
        }
    }
    m += "\n\n<span alpha='70%'>Take a box</span>";
    for (std::size_t i = 0; i < a.boxes.size() && i < 3; ++i) {
        const auto& b = a.boxes[i];
        m += "\n  " + std::string(model::name(b.c)) + " " + std::to_string(b.points) +
             (b.gain > b.points ? " (+" + std::to_string(b.gain - b.points) + ")" : "") +
             "  <span alpha='70%'>\u2192 " + num(b.expect) + "</span>";
    }
    m_advice.set_markup(m);
}

}  // namespace yatze
