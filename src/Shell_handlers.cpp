#include "Shell.hpp"
#include "AboutWindow.hpp"
#include "NewGameDialog.hpp"
#include "model/Computer.hpp"
#include "model/TableFile.hpp"
#include "ShortcutsDialog.hpp"
#include "Log.hpp"

// Shell_handlers.cpp -- HANDLERS. Each game handler is ONE call into
// model::verbs, which goes through the journal; the repaint follows from the
// store's change signal. A handler never writes the game itself.

namespace yatze {

void Shell::on_new_game() {  // handler: Ctrl+N -- same table again
    start_game(m_seats);
}

void Shell::on_new_game_dialog() {  // handler: Ctrl+Shift+N -- who plays
    if (!m_newgame) {
        m_newgame = std::make_unique<NewGameDialog>(*this);
        m_newgame->signal_start().connect(sigc::mem_fun(*this, &Shell::start_game));
    }
    m_newgame->open(m_seats);
}

void Shell::start_game(std::vector<model::Player> seats) {  // handler
    m_seats = seats;
    model::table_file::save(table_file(), m_seats);   // remembered for next launch
    m_cpu_tick.disconnect();              // a computer step from the old game must not land
    ++m_game_serial;
    m_recorded = false;
    m_news.clear();
    model::verbs::new_game(m_store, m_journal, std::move(seats));
    m_said.set_text("");
    if (auto lg = log::get(log::Area::Game)) lg->info("new game: {} seat(s)", m_seats.size());
}

// Input from the person at the table is only taken on a person's turn: the
// keys and clicks would otherwise move the computer's dice.
void Shell::on_roll() {  // handler: R / button
    if (!human_turn() || m_dice3d.busy()) return;
    if (model::verbs::roll(m_store, m_journal)) {
        m_said.set_text("");
        if (auto lg = log::get(log::Area::Game))
            lg->info("roll {} -> {}{}{}{}{}", m_store.state().rolls_used,
                     m_store.state().dice[0], m_store.state().dice[1], m_store.state().dice[2],
                     m_store.state().dice[3], m_store.state().dice[4]);
    }
}

void Shell::on_hold(int die) {  // handler: 1-5 / click a die
    if (!human_turn() || m_dice3d.busy()) return;
    model::verbs::toggle_hold(m_store, m_journal, die);
}

void Shell::on_score(model::Category c) {  // handler: click a box
    if (!human_turn() || m_dice3d.busy()) return;
    if (model::verbs::score(m_store, m_journal, c)) {
        m_said.set_text(m_journal.undo_label());
        if (auto lg = log::get(log::Area::Game)) lg->info("{}", m_journal.undo_label());
    }
}

void Shell::on_undo() {  // handler
    if (!human_turn()) return;            // never undo into the middle of a computer's turn
    const auto r = m_journal.undo();
    if (r.done) m_said.set_text("Undid: " + r.label);
    if (auto lg = log::get(log::Area::Game)) lg->info("undo: {}", r.done ? r.label : "(nothing)");
}

void Shell::on_redo() {  // handler
    if (!human_turn()) return;
    const auto r = m_journal.redo();
    if (r.done) m_said.set_text("Redid: " + r.label);
    if (auto lg = log::get(log::Area::Game)) lg->info("redo: {}", r.done ? r.label : "(nothing)");
}

// One step of a computer turn (model::computer decides; the verbs apply it,
// through the journal like any move). After the computer takes its box the
// journal is cleared: the next person cannot undo the computer's choice.
void Shell::on_computer_step() {  // handler
    namespace cpu = model::computer;
    const auto step = cpu::decide(m_store.state(), m_engine.ready() ? &m_engine.table() : nullptr);
    if (step.kind == cpu::Step::Kind::None) { schedule_computer(); return; }   // waiting for the odds
    const bool scored = step.kind == cpu::Step::Kind::Score;
    const std::string who = m_store.state().player().name;
    cpu::apply(m_store, m_journal, step);
    if (scored) {
        if (auto lg = log::get(log::Area::Game)) lg->info("{}: {}", who, m_journal.undo_label());
        m_said.set_text(who + ": " + m_journal.undo_label());
        m_journal.clear();
    }
}

// ── the drawers: model::Drawers decides, apply_drawers paints ───────────────
void Shell::on_toggle_left_drawer()  { if (m_drawers.toggle(model::Drawers::Side::Left))  apply_drawers(); }
void Shell::on_toggle_right_drawer() { if (m_drawers.toggle(model::Drawers::Side::Right)) apply_drawers(); }
void Shell::on_close_drawers()       { if (m_drawers.close()) apply_drawers(); }

void Shell::on_dice_style(const Glib::ustring& k) {  // handler: menu › Red / Ivory dice
    model::settings::DiceStyle d;
    if (!model::settings::parse(k, d)) return;
    m_act_dice->change_state(k);
    m_settings.dice = d;
    model::settings::save(settings_file(), m_settings);
    apply_settings();
}

// ── hide-on-close singletons (Cairn's dialog-lifetime stone) ────────────────
void Shell::on_about() {
    if (!m_about) m_about = std::make_unique<AboutWindow>(*this);
    m_about->present();
}

void Shell::on_shortcuts() {
    if (!m_shortcuts) m_shortcuts = std::make_unique<ShortcutsDialog>();
    m_shortcuts->show(*this);
}

}  // namespace yatze
