#pragma once
#include "widgets/Widgets.hpp"

// HintPanel -- the LEFT drawer's content: the rules, always there to read.
// Live hints for the current roll (which boxes are strong, what to hold) come
// with the strategy engine; this panel gains a "This roll" section then.
namespace yatze {

class HintPanel : public widgets::Box {
public:
    HintPanel();
};

}  // namespace yatze
