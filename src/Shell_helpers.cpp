#include "Shell.hpp"
#include "Log.hpp"
#include "model/Computer.hpp"

#include <glibmm/main.h>
#include <glibmm/miscutils.h>
#include <glibmm/datetime.h>

#include <algorithm>
#include <cstdlib>
#include <gtkmm/editable.h>
#include <gtkmm/textview.h>

// Shell_helpers.cpp -- HELPERS. Paint the window from the model; nothing here
// writes the game.

namespace yatze {

// Several writes in one handler (or an undo) repaint once, after they settle.
void Shell::queue_repaint() {  // helper
    if (m_repaint_queued) return;
    m_repaint_queued = true;
    Glib::signal_idle().connect_once([this] {
        m_repaint_queued = false;
        repaint();
    });
}

std::string Shell::settings_file() const {  // helper
    return Glib::build_filename(Glib::get_user_config_dir(), "yatze", "settings.json");
}

void Shell::apply_settings() {  // helper
    const bool red = m_settings.dice == model::settings::DiceStyle::Red;
    m_dice3d.set_red(red);
    m_dice.set_red(red);
}

std::string Shell::history_file() const {  // helper
    return Glib::build_filename(Glib::get_user_data_dir(), "yatze", "history.json");
}

// The rising edge of game over: record the game once, make the last box final
// (no undoing a recorded game), say who won and any new best, and open the
// standings.
void Shell::check_game_over() {  // helper
    const auto& g = m_store.state();
    if (!g.over() || m_recorded || g.players.empty()) return;
    m_recorded = true;
    m_journal.clear();
    const std::string now = Glib::DateTime::create_now_local().format("%Y-%m-%d %H:%M").raw();
    auto rec = model::stats::record(g, now);
    m_history.add(rec);
    m_history.save(history_file());

    const auto lead = g.leaders();
    if (g.players.size() == 1) m_news = std::to_string(g.players[0].card.total()) + " points";
    else if (lead.size() > 1)  m_news = "A tie at " + std::to_string(g.players[lead[0]].card.total());
    else m_news = g.players[lead[0]].name + " wins with " + std::to_string(g.players[lead[0]].card.total());
    // A person's new best (only once they have a record to beat).
    for (const auto& s : rec.seats) {
        if (s.seat != model::Seat::Human) continue;
        const int prev = m_history.previous_best(model::stats::key(s));
        if (prev > 0 && s.total > prev) {
            m_news += " \u2014 a new best for " + s.name + "!";
            break;
        }
    }
    if (const auto top = m_history.top(1); !top.empty() && top[0].when == now && m_history.games().size() > 1 &&
        std::any_of(rec.seats.begin(), rec.seats.end(), [&](const auto& s) { return s.total == top[0].total; }))
        m_news += " Top score!";
    m_said.set_text(m_news);
    if (auto lg = log::get(log::Area::Game)) lg->info("game over: {}", m_news);
    if (!m_drawers.is_open(model::Drawers::Side::Right) && m_drawers.show(model::Drawers::Side::Right)) apply_drawers();
}

std::string Shell::table_file() const {  // helper: the UI resolves the path; the model reads it
    return Glib::build_filename(Glib::get_user_config_dir(), "yatze", "table.json");
}

bool Shell::human_turn() const {  // helper
    const auto& g = m_store.state();
    return !g.players.empty() && !g.over() && g.player().seat == model::Seat::Human;
}

// The pace of a computer turn: long enough to see each thing it does. One
// step per tick; the step's own change repaints and schedules the next.
void Shell::schedule_computer() {  // helper
    const auto& g = m_store.state();
    if (m_cpu_tick.connected() || g.players.empty() || g.over() || human_turn()) return;
    if (m_dice3d.busy()) return;                 // its roll is still playing; signal_settled re-schedules
    namespace cpu = model::computer;
    const auto next = cpu::decide(g, m_engine.ready() ? &m_engine.table() : nullptr);
    int ms = 250;                                             // waiting for the odds: poll
    switch (next.kind) {
        case cpu::Step::Kind::Roll:  ms = g.rolls_used == 0 ? 900 : 700; break;
        case cpu::Step::Kind::Hold:  ms = 380;  break;
        case cpu::Step::Kind::Score: ms = 1100; break;       // let you see the dice it scores
        case cpu::Step::Kind::None:  break;
    }
    static const double speed = [] {   // YATZE_SPEED: testing only (see DiceView::tick)
        const char* e = std::getenv("YATZE_SPEED");
        const double v = e ? std::atof(e) : 1.0;
        return v > 0 ? v : 1.0;
    }();
    m_cpu_tick = Glib::signal_timeout().connect([this] {
        on_computer_step();
        return false;
    }, std::max(10, int(ms / speed)));
}

void Shell::repaint() {  // helper: dice, card, status, actions
    const auto& g = m_store.state();
    const bool human = human_turn();
    const bool rolling = m_dice3d.busy();       // the take is playing: nothing new is revealed yet
    m_dice.set(g.dice, g.held, human && model::verbs::can_hold(g));
    // The roll key changes exactly when new dice are rolled: game, round, seat, roll.
    const std::uint64_t key = (m_game_serial << 24) | (std::uint64_t(g.round) << 16) |
                              (std::uint64_t(g.current) << 8) | std::uint64_t(g.rolls_used);
    m_dice3d.set(g.dice, g.held, human && model::verbs::can_hold(g), key);
    const bool rolling_now = m_dice3d.busy();   // set() may just have started a take
    m_card.set(g, human && !rolling_now, !rolling_now);
    m_status.set_text(rolling_now ? std::string("Rolling\u2026") : status_text());
    if (m_act_roll) m_act_roll->set_enabled(human && !rolling_now && model::verbs::can_roll(g));
    (void)rolling;
    update_undo_ui();
    schedule_computer();
    m_roll.set_label(g.over() ? "Game over" : g.rolls_used == 0 ? "Roll"
                     : "Roll  (" + std::to_string(g.rolls_left()) + " left)");
    check_game_over();
    if (m_drawers.is_open(model::Drawers::Side::Right)) m_stats.show(g, m_history, m_news);
    refresh_hints();
}

// The advice, and its glow (Scott, s004): while the hints drawer is open on
// your turn, the dice it says to keep glow amber on the table, or the box it
// says to take is outlined on the card. Closed drawer: no advice, no glow.
void Shell::refresh_hints() {  // helper
    const auto& g = m_store.state();
    std::array<bool, 5> glow{};
    int box = -1;
    const bool open = m_drawers.is_open(model::Drawers::Side::Left);
    const bool rolling = m_dice3d.busy();
    if (open) m_hints.show(g, m_engine, rolling);
    if (open && !rolling && human_turn() && m_engine.ready() && g.rolls_used > 0) {
        const auto a = model::strategy::advise(m_engine.table(), g.player().card, g.dice, g.rolls_left());
        if (a.roll_on && !a.holds.empty()) glow = a.holds.front().hold;
        else if (!a.boxes.empty()) box = model::index(a.boxes.front().c);
    }
    m_dice3d.set_glow(glow);
    m_dice.set_glow(glow);
    m_card.set_suggested(box);
}

std::string Shell::status_text() const {  // helper
    const auto& g = m_store.state();
    if (g.players.empty()) return "";
    if (g.over()) {
        const auto lead = g.leaders();
        const int top = g.players[lead.front()].card.total();
        if (g.players.size() == 1) return "Game over — " + std::to_string(top) + " points";
        if (lead.size() > 1) return "Game over — a tie at " + std::to_string(top);
        return "Game over — " + g.players[lead.front()].name + " wins with " + std::to_string(top);
    }
    if (g.player().seat == model::Seat::Computer) {
        std::string doing = g.rolls_used == 0 ? "about to roll" : "thinking";
        if (g.player().level != model::Level::Easy && !m_engine.ready()) doing = "waiting for the odds";
        return g.player().name + " (" + model::computer::level_name(g.player().level) + ") \u2014 Round " +
               std::to_string(g.round) + " of 13 \u2014 " + doing + "\u2026";
    }
    std::string who = g.players.size() == 1 ? "" : g.player().name + " — ";
    std::string what = g.rolls_used == 0 ? "roll the dice"
                     : g.rolls_left() == 0 ? "take a box"
                     : "hold, roll again, or take a box";
    return who + "Round " + std::to_string(g.round) + " of 13 — " + what;
}

void Shell::update_undo_ui() {  // helper
    const bool human = human_turn();
    if (m_act_undo) m_act_undo->set_enabled(human && m_journal.can_undo());
    if (m_act_redo) m_act_redo->set_enabled(human && m_journal.can_redo());
    const std::string u = m_journal.undo_label(), r = m_journal.redo_label();
    if (m_edit_menu) {
        m_edit_menu->remove_all();
        m_edit_menu->append(u.empty() ? "Undo" : "Undo " + u, "win.undo");
        m_edit_menu->append(r.empty() ? "Redo" : "Redo " + r, "win.redo");
    }
}

// Paint the drawers from m_drawers -- the one place a revealer moves. The
// standings read as the drawer opens; on close the keyboard returns to play.
void Shell::apply_drawers() {  // helper
    using Side = model::Drawers::Side;
    if (m_drawers.is_open(Side::Right)) m_stats.show(m_store.state(), m_history, m_news);
    refresh_hints();
    m_left_drawer.set_reveal_child(m_drawers.is_open(Side::Left));
    // The hints drawer covers the left of the table: let the dice make room.
    float inset = 0;
    if (m_drawers.is_open(Side::Left)) {
        // The view starts at the play area's left margin (the play area is the
        // first thing in the content row); the drawer is its panel's width.
        int rw = 0, rh = 0;
        m_hints.get_size_request(rw, rh);
        const int drawer_w = std::max(m_hints.get_width(), rw);
        inset = float(drawer_w - m_play.get_margin_start());
    }
    m_dice3d.set_left_inset(inset);
    // The standings drawer covers the right of the window; if it reaches over
    // the table, the row moves left the same way.
    float inset_r = 0;
    if (m_drawers.is_open(Side::Right)) {
        int rw = 0, rh = 0;
        m_stats.get_size_request(rw, rh);
        const int drawer_w = std::max(m_stats.get_width(), rw);
        const int drawer_left = m_overlay.get_width() - drawer_w;
        const int view_right = m_play.get_width() - m_play.get_margin_end();
        inset_r = float(std::max(0, view_right - drawer_left));
    }
    m_dice3d.set_right_inset(inset_r);
    m_right_drawer.set_reveal_child(m_drawers.is_open(Side::Right));
    m_scrim.set_visible(m_drawers.scrim());
    if (!m_drawers.scrim()) m_roll.grab_focus();
}

bool Shell::focus_is_text() const {  // helper
    for (const Gtk::Widget* w = get_focus(); w; w = w->get_parent())
        if (dynamic_cast<const Gtk::TextView*>(w) || dynamic_cast<const Gtk::Editable*>(w)) return true;
    return false;
}

}  // namespace yatze
