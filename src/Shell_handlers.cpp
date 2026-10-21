#include "Shell.hpp"
#include "AboutWindow.hpp"
#include "ShortcutsDialog.hpp"
#include "Log.hpp"

// Shell_handlers.cpp -- HANDLERS. Each game handler is ONE call into
// model::verbs, which goes through the journal; the repaint follows from the
// store's change signal. A handler never writes the game itself.

namespace yatze {

void Shell::on_new_game(int players) {  // handler: 0 = same table again
    if (players > 0) m_table = players;
    std::vector<model::Player> ps;
    if (m_table == 1) ps.push_back({"You"});
    else for (int i = 0; i < m_table; ++i) ps.push_back({"Player " + std::to_string(i + 1)});
    model::verbs::new_game(m_store, m_journal, std::move(ps));
    m_said.set_text("");
    if (auto lg = log::get(log::Area::Game)) lg->info("new game: {} player(s)", m_table);
}

void Shell::on_roll() {  // handler: R / button
    if (model::verbs::roll(m_store, m_journal)) {
        m_said.set_text("");
        if (auto lg = log::get(log::Area::Game))
            lg->info("roll {} -> {}{}{}{}{}", m_store.state().rolls_used,
                     m_store.state().dice[0], m_store.state().dice[1], m_store.state().dice[2],
                     m_store.state().dice[3], m_store.state().dice[4]);
    }
}

void Shell::on_hold(int die) {  // handler: 1-5 / click a die
    model::verbs::toggle_hold(m_store, m_journal, die);
}

void Shell::on_score(model::Category c) {  // handler: click a box
    if (model::verbs::score(m_store, m_journal, c)) {
        m_said.set_text(m_journal.undo_label());
        if (auto lg = log::get(log::Area::Game)) lg->info("{}", m_journal.undo_label());
    }
}

void Shell::on_undo() {  // handler
    const auto r = m_journal.undo();
    if (r.done) m_said.set_text("Undid: " + r.label);
    if (auto lg = log::get(log::Area::Game)) lg->info("undo: {}", r.done ? r.label : "(nothing)");
}

void Shell::on_redo() {  // handler
    const auto r = m_journal.redo();
    if (r.done) m_said.set_text("Redid: " + r.label);
    if (auto lg = log::get(log::Area::Game)) lg->info("redo: {}", r.done ? r.label : "(nothing)");
}

// ── the drawers: model::Drawers decides, apply_drawers paints ───────────────
void Shell::on_toggle_left_drawer()  { if (m_drawers.toggle(model::Drawers::Side::Left))  apply_drawers(); }
void Shell::on_toggle_right_drawer() { if (m_drawers.toggle(model::Drawers::Side::Right)) apply_drawers(); }
void Shell::on_close_drawers()       { if (m_drawers.close()) apply_drawers(); }

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
