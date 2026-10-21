#pragma once
#include "model/Rules.hpp"
#include "widgets/Widgets.hpp"

#include <array>
#include <sigc++/signal.h>

// DiceRow -- the five dice, flat (s001). Each die is a button drawn with Cairo;
// a click asks to hold / release it (signal_die). The row only SHOWS the model:
// set() paints faces and holds, and nothing here changes a die by itself.
//
// The 3D table (DiceView, a GLArea) replaces this row later; the Shell talks to
// it through the same three calls, so the swap touches one zone.
namespace yatze {

class DiceRow : public widgets::Box {
public:
    DiceRow();

    // Faces (0 = not rolled), which are held, whether holding is allowed now.
    void set(const model::Dice& dice, const std::array<bool, 5>& held, bool can_hold);
    sigc::signal<void(int)>& signal_die() { return m_sig_die; }

private:
    void draw_die(const Cairo::RefPtr<Cairo::Context>& cr, int w, int h, int i);

    std::array<widgets::Button*, 5>      m_buttons{};
    std::array<widgets::DrawingArea*, 5> m_faces{};
    model::Dice                          m_dice{};
    std::array<bool, 5>                  m_held{};
    sigc::signal<void(int)>              m_sig_die;
};

}  // namespace yatze
