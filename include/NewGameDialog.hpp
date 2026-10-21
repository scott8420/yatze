#pragma once
#include "model/Game.hpp"
#include "widgets/Widgets.hpp"

#include <gtkmm/window.h>
#include <sigc++/signal.h>

#include <vector>

// NewGameDialog -- who sits at the table. One row per seat: a name, and who
// plays it (a person, or the computer at Easy / Medium / Hard). Add a seat,
// remove one, Start. Seat order is turn order.
//
// Hide-on-close singleton (Cairn's dialog-lifetime stone): the Shell builds it
// once and re-presents it with the current table; Start emits the new one.
namespace yatze {

class NewGameDialog : public Gtk::Window {
public:
    static constexpr int kMaxSeats = 6;

    explicit NewGameDialog(Gtk::Window& parent);
    ~NewGameDialog() override;

    void open(const std::vector<model::Player>& table);   // fill the rows, present
    sigc::signal<void(std::vector<model::Player>)>& signal_start() { return m_start; }

private:
    struct Row;
    void add_row(const model::Player& p);
    void remove_row(Row* r);
    void refresh_buttons();
    std::vector<model::Player> table() const;

    widgets::Box     m_rows;
    widgets::Button  m_add;
    widgets::Button  m_go;
    std::vector<std::unique_ptr<Row>> m_seats;
    sigc::signal<void(std::vector<model::Player>)> m_start;
};

}  // namespace yatze
