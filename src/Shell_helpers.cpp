#include "Shell.hpp"
#include "A11y.hpp"
#include "Log.hpp"
#include "model/Computer.hpp"

#include <gdkmm/frameclock.h>
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
    // HIGH_IDLE runs ahead of GTK's redraw: while the table animates every
    // frame (a slow GPU, or the shadow pass on a software renderer), a plain
    // idle could starve and the game would stall waiting for its repaint.
    Glib::signal_idle().connect_once([this] {
        m_repaint_queued = false;
        repaint();
    }, Glib::PRIORITY_HIGH_IDLE);
}

std::string Shell::settings_file() const {  // helper
    return Glib::build_filename(Glib::get_user_config_dir(), "yatze", "settings.json");
}

void Shell::apply_settings() {  // helper
    const bool red = m_settings.dice == model::settings::DiceStyle::Red;
    m_dice3d.set_red(red);
    m_dice.set_red(red);
    m_dice3d.set_sound(m_settings.sound);
    m_sound_button.set_icon_name(m_settings.sound ? "audio-volume-high-symbolic" : "audio-volume-muted-symbolic");
    a11y::label(m_sound_button, m_settings.sound ? "Sound on" : "Sound off");
    m_sound_button.set_tooltip_text(m_settings.sound ? "Sound on  (M to mute)" : "Sound off  (M to turn on)");
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
    m_dice3d.celebrate(gfx::Cheer::GameOver);
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
    if (m_dice3d.busy()) {                       // its roll is still playing; signal_settled re-schedules
        if (auto lg = log::get(log::Area::Game)) lg->debug("computer: waits for the dice");
        return;
    }
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
    if (computer_quick() && next.kind != cpu::Step::Kind::None) ms = int(ms / kQuickRate);
    if (auto lg = log::get(log::Area::Game)) lg->debug("computer: next step ({}) in {} ms", int(next.kind), ms);
    m_cpu_tick = Glib::signal_timeout().connect([this] {
        on_computer_step();
        return false;
    }, std::max(10, int(ms / speed)));
}

void Shell::repaint() {  // helper: dice, card, status, actions
    if (auto lg = log::get(log::Area::Shell)) lg->debug("repaint");
    const auto& g = m_store.state();
    const bool human = human_turn();
    const bool rolling = m_dice3d.busy();       // the take is playing: nothing new is revealed yet
    m_dice.set(g.dice, g.held, human && model::verbs::can_hold(g));
    // The roll key changes exactly when new dice are rolled: game, round, seat, roll.
    const std::uint64_t key = (m_game_serial << 24) | (std::uint64_t(g.round) << 16) |
                              (std::uint64_t(g.current) << 8) | std::uint64_t(g.rolls_used);
    // s017: on a computer's turn, Quick pace plays its rolls about twice as fast.
    m_dice3d.set_rate(computer_quick() ? kQuickRate : 1.0);
    m_dice3d.set(g.dice, g.held, human && model::verbs::can_hold(g), key);
    a11y::label(m_dice3d, dice_words(g, m_dice3d.busy()));
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
    announce_turn();
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

// The dice in words, for a screen reader (s018; the words are the model's).
std::string Shell::dice_words(const model::GameState& g, bool rolling) {  // helper
    return rolling ? std::string("Dice: rolling") : model::dice_words(g.dice, g.held);
}

bool Shell::computer_quick() const {  // helper: a computer is playing, at Quick pace
    const auto& g = m_store.state();
    return m_settings.pace == model::settings::Pace::Quick && !g.players.empty() && !g.over() &&
           g.player().seat == model::Seat::Computer;
}

double Shell::test_speed() {  // helper: YATZE_SPEED=n runs the table n times faster (testing only)
    static const double speed = [] {
        const char* e = std::getenv("YATZE_SPEED");
        const double v = e ? std::atof(e) : 1.0;
        return v > 0 ? v : 1.0;
    }();
    return speed;
}

// A score lands (s014): "+25" rises off the table and arcs into its box on
// the card, which flashes as it arrives. The box is looked up every frame
// (the card rebuilds itself), so the flight always ends on it.
void Shell::fly_score(int player, int box, int points) {  // helper
    m_fly.set_text(points > 0 ? "+" + std::to_string(points) : "0");
    {
        const auto& g = m_store.state();
        const std::string who = player >= 0 && player < int(g.players.size()) ? g.players[std::size_t(player)].name : "";
        a11y::announce(*this, (who == "You" ? std::string("You took ") : who + " took ") + std::to_string(points) +
                              " in " + std::string(model::name(model::category(box))));
    }
    if (points > 0) m_fly.remove_css_class("nothing"); else m_fly.add_css_class("nothing");
    m_fly_player = player;
    m_fly_box = box;
    m_fly_t0 = -1;
    m_fly.set_opacity(0);
    m_fly.set_visible(true);
    if (m_fly_tick) return;                      // one flight at a time: the new one takes over
    m_fly_tick = m_overlay.add_tick_callback([this](const Glib::RefPtr<Gdk::FrameClock>& clock) {
        const gint64 now = clock->get_frame_time();
        if (m_fly_t0 < 0) m_fly_t0 = now;
        const double dur = 0.75 / test_speed();
        const double u = std::min(1.0, double(now - m_fly_t0) / 1e6 / dur);
        Gtk::Widget* target = m_card.box_cell(m_fly_player, m_fly_box);
        double sx = 0, sy = 0, tx = 0, ty = 0;
        const int fw = m_fly.get_width(), fh = m_fly.get_height();
        bool ok = target != nullptr;
        if (ok) {
            // (C API: gtkmm 4.10 has no compute_point wrapper; GTK has had it since 4.12.)
            const graphene_point_t pa = GRAPHENE_POINT_INIT(float(m_table.get_width()) / 2, float(m_table.get_height()) * 0.55f);
            const graphene_point_t pb = GRAPHENE_POINT_INIT(float(target->get_width()) / 2, float(target->get_height()) / 2);
            graphene_point_t a, b;
            ok = gtk_widget_compute_point(GTK_WIDGET(m_table.gobj()), GTK_WIDGET(m_overlay.gobj()), &pa, &a) &&
                 gtk_widget_compute_point(target->gobj(), GTK_WIDGET(m_overlay.gobj()), &pb, &b);
            if (ok) { sx = a.x; sy = a.y; tx = b.x; ty = b.y; }
        }
        if (!ok || u >= 1.0) {
            m_fly.set_visible(false);
            if (target) m_card.flash(m_fly_player, m_fly_box);
            m_fly_tick = 0;
            return false;
        }
        const double e = u * u * (3 - 2 * u);
        const double lift = 90.0 * std::sin(u * 3.14159265);       // an arc, up and over
        const double x = sx + (tx - sx) * e, y = sy + (ty - sy) * e - lift;
        m_fly.set_margin_start(std::max(0, int(x - fw / 2.0)));
        m_fly.set_margin_top(std::max(0, int(y - fh / 2.0)));
        m_fly.set_opacity(std::min(1.0, u / 0.12) * (u > 0.85 ? (1.0 - u) / 0.15 : 1.0));
        return true;
    });
}

// The turn has passed (s013): with two or more seats, a card over the table
// says whose turn it is -- name, round, their score so far -- for 1.8 s.
void Shell::announce_turn() {  // helper
    const auto& g = m_store.state();
    if (g.players.size() < 2 || g.over() || g.rolls_used != 0) return;
    const std::uint64_t seen = (m_game_serial << 16) | (std::uint64_t(g.round) << 8) | std::uint64_t(g.current);
    if (seen == m_turn_seen) return;
    m_turn_seen = seen;
    const auto& p = g.player();
    // "You" is the default name for a person: "Your turn", not "You's turn".
    const std::string whose = p.name == "You" ? "Your turn" : p.name + "\u2019s turn";
    m_turn_name.set_text(p.seat == model::Seat::Human ? whose : whose + " (computer)");
    m_turn_info.set_text("Round " + std::to_string(g.round) + " of 13  \u00b7  " + std::to_string(p.card.total()) + " points");
    // If a score is still flying off the table, let it land first.
    const double speed = test_speed();
    const int wait = m_fly_tick ? int(700 / speed) : 0;
    m_turn_hide.disconnect();
    m_turn_hide = Glib::signal_timeout().connect([this, speed] {
        m_turn_reveal.set_reveal_child(true);
        m_turn_hide = Glib::signal_timeout().connect([this] { m_turn_reveal.set_reveal_child(false); return false; },
                                                     std::max(10, int(1800 / speed)));
        return false;
    }, std::max(1, wait));
    a11y::announce(*this, m_turn_name.get_text().raw() + ", " + m_turn_info.get_text().raw());
    if (auto lg = log::get(log::Area::Game)) lg->debug("turn card: {}", m_turn_name.get_text().raw());
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
    const std::string cheer = g.rolls_used > 0 && model::is_yatze(g.dice) ? "Yatz\u00e9! " : "";
    if (!cheer.empty()) {
        std::string who = g.players.size() == 1 ? "" : g.player().name + " \u2014 ";
        return cheer + who + "five " + std::to_string(g.dice[0]) + "s";
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
