#include "NewGameDialog.hpp"
#include "Registry.hpp"

#include <gtkmm/dropdown.h>
#include <gtkmm/entry.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/label.h>
#include <gtkmm/stringlist.h>

#include <algorithm>

namespace yatze {

namespace {
// The "plays as" choices, in DropDown order.
const char* const kWho[] = {"Person", "Computer — Easy", "Computer — Medium", "Computer — Hard"};
int who_index(const model::Player& p) {
    if (p.seat == model::Seat::Human) return 0;
    return 1 + int(p.level);
}
}  // namespace

struct NewGameDialog::Row {
    Gtk::Box      box{Gtk::Orientation::HORIZONTAL, 8};
    Gtk::Entry    name;
    Gtk::DropDown who;
    Gtk::Button   remove;
};

NewGameDialog::NewGameDialog(Gtk::Window& parent)
    : m_rows(widgets::unregistered, "newgame.rows", Gtk::Orientation::VERTICAL, 6),
      m_add(widgets::unregistered, "newgame.add"),
      m_go(widgets::unregistered, "newgame.start") {
    set_name("newgame");
    registry::add("newgame", this);
    set_title("New Game");
    set_modal(true);
    set_transient_for(parent);
    set_hide_on_close(true);
    set_default_size(460, -1);

    auto* hb = Gtk::make_managed<Gtk::HeaderBar>();
    auto* cancel = Gtk::make_managed<Gtk::Button>("Cancel");
    cancel->signal_clicked().connect([this] { close(); });
    hb->pack_start(*cancel);
    m_go.set_label("Start");
    m_go.add_css_class("suggested-action");
    m_go.signal_clicked().connect([this] {
        m_start.emit(table());
        close();
    });
    hb->pack_end(m_go);
    hb->set_show_title_buttons(false);
    set_titlebar(*hb);

    auto* page = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 12);
    page->set_margin(18);
    auto* intro = Gtk::make_managed<Gtk::Label>("Who is playing? Seats take turns from the top.");
    intro->set_xalign(0.0f);
    intro->add_css_class("dim-label");
    page->append(*intro);
    page->append(m_rows);
    m_add.set_label("Add a seat");
    m_add.set_halign(Gtk::Align::START);
    m_add.signal_clicked().connect([this] {
        int cpu = 0;
        for (const auto& r : m_seats) if (r->who.get_selected() > 0) ++cpu;
        add_row({"Computer " + std::to_string(cpu + 1), model::Seat::Computer, model::Level::Medium});
        refresh_buttons();
    });
    page->append(m_add);
    set_child(*page);
}

NewGameDialog::~NewGameDialog() { registry::remove(this); }

void NewGameDialog::open(const std::vector<model::Player>& table) {
    for (auto& r : m_seats) m_rows.remove(r->box);
    m_seats.clear();
    for (const auto& p : table) add_row(p);
    if (m_seats.empty()) add_row({"You"});
    refresh_buttons();
    present();
    m_go.grab_focus();
}

void NewGameDialog::add_row(const model::Player& p) {
    auto r = std::make_unique<Row>();
    r->name.set_text(p.name);
    r->name.set_hexpand(true);
    r->name.set_placeholder_text("Name");
    r->name.signal_changed().connect([this] { refresh_buttons(); });
    r->name.signal_activate().connect([this] { if (m_go.get_sensitive()) m_go.activate(); });
    std::vector<Glib::ustring> who(std::begin(kWho), std::end(kWho));
    r->who.set_model(Gtk::StringList::create(who));
    r->who.set_selected(guint(who_index(p)));
    r->who.set_size_request(200, -1);     // same width every row, so the names line up
    r->remove.set_icon_name("list-remove-symbolic");
    r->remove.set_tooltip_text("Remove this seat");
    r->remove.add_css_class("flat");
    Row* raw = r.get();
    r->remove.signal_clicked().connect([this, raw] { remove_row(raw); });
    r->box.append(r->name);
    r->box.append(r->who);
    r->box.append(r->remove);
    m_rows.append(r->box);
    m_seats.push_back(std::move(r));
}

void NewGameDialog::remove_row(Row* r) {
    if (m_seats.size() <= 1) return;
    m_rows.remove(r->box);
    m_seats.erase(std::remove_if(m_seats.begin(), m_seats.end(),
                                 [r](const auto& p) { return p.get() == r; }), m_seats.end());
    refresh_buttons();
}

void NewGameDialog::refresh_buttons() {
    m_add.set_sensitive(int(m_seats.size()) < kMaxSeats);
    for (auto& r : m_seats) r->remove.set_sensitive(m_seats.size() > 1);
    bool named = true;
    for (auto& r : m_seats) if (r->name.get_text().empty()) named = false;
    m_go.set_sensitive(named);
}

std::vector<model::Player> NewGameDialog::table() const {
    std::vector<model::Player> out;
    for (const auto& r : m_seats) {
        model::Player p;
        p.name = r->name.get_text();
        const int w = int(r->who.get_selected());
        p.seat  = w == 0 ? model::Seat::Human : model::Seat::Computer;
        p.level = w <= 1 ? model::Level::Easy : w == 2 ? model::Level::Medium : model::Level::Hard;
        if (p.seat == model::Seat::Human) p.level = model::Level::Medium;
        out.push_back(p);
    }
    return out;
}

}  // namespace yatze
