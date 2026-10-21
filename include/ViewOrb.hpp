#pragma once
#include "widgets/Widgets.hpp"

// ViewOrb -- the navigation orb over the 3D table (s008; Scott: "an orb with
// the x, y, z that the user can pivot"), after Blender's view gizmo. It draws
// the world axes as the table's camera sees them -- X red, Y green, Z blue,
// a lettered ball on each positive end, a plain one on each negative end,
// nearer ones in front -- and turns with every frame.
//   drag     -> orbit the camera around the dice
//   click    -> a ball: look along that axis
// The reset button beside it (the Shell's) goes back to the standard view.
namespace yatze {

class DiceView;

class ViewOrb : public widgets::DrawingArea {
public:
    explicit ViewOrb(DiceView& view);

private:
    struct Ball { int axis; double x, y, r; float depth; };
    void draw(const Cairo::RefPtr<Cairo::Context>& cr, int w, int h);
    std::vector<Ball> balls(int w, int h) const;

    DiceView& m_view;
    bool      m_hover = false;
    double    m_last_x = 0, m_last_y = 0, m_moved = 0;
    double    m_press_x = 0, m_press_y = 0;
};

}  // namespace yatze
