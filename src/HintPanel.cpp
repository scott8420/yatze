#include "HintPanel.hpp"
#include "model/Rules.hpp"

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

HintPanel::HintPanel() : widgets::Box("drawer.hints", Gtk::Orientation::VERTICAL, 8) {
    add_css_class("background");
    add_css_class("drawer-panel");
    add_css_class("left");
    set_size_request(300, -1);

    auto* title = Gtk::make_managed<Gtk::Label>("Rules");
    title->set_xalign(0.0f);
    title->add_css_class("title-4");
    append(*title);

    auto* body = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
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
        "<b>Undo</b> (Ctrl+Z) takes back a mis-click. The dice are fated: undo a roll "
        "and roll again, and the same dice come up.", true));
    body->append(*para("<i>Hints for the current roll arrive with the computer player.</i>", true));

    auto* scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    scroll->set_vexpand(true);
    scroll->set_child(*body);
    append(*scroll);
}

}  // namespace yatze
