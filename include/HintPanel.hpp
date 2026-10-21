#pragma once
#include "model/Game.hpp"
#include "widgets/Widgets.hpp"

// HintPanel -- the LEFT drawer: "This roll" (live advice from the strategy
// engine) above the rules. The advice is the engine's exact best play for the
// current player's card: which dice to keep and roll, or which box to take,
// each with the final score it leads to on average.
namespace yatze {

class Engine;

class HintPanel : public widgets::Box {
public:
    HintPanel();
    // Re-read the advice for this position (cheap: one turn, solved exactly).
    void show(const model::GameState& g, const Engine& engine);

private:
    widgets::Label m_advice;
};

}  // namespace yatze
