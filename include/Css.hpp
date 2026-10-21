#pragma once

namespace yatze {

// The app stylesheet, installed once in App::on_activate (after the light/dark
// choice, so `currentColor` and `.background` below already mean the right thing).
//
// The drawer tabs are invisible-until-hover ENTIRELY IN CSS: opacity 0 with a
// :hover override and a transition. The widget is always present and always
// clickable; CSS only fades it in. Toggling opacity / visibility from C++ on
// motion was tried in Folio and abandoned -- this is the reliable idiom.
//
// No colour literals and no named theme colours (@window_bg_color is not
// defined by every GTK theme -- in the sandbox it silently dropped the rule and
// the panel went transparent). The panel takes the theme's own `.background`
// class (added in C++), and everything else is `currentColor` at an alpha, so
// the drawers follow the desktop's light/dark preference with the window.
inline const char* YATZE_CSS = R"CSS(
    /* The pull tabs: always present, quiet until hovered. */
    .drawer-tab, .drawer-tab-right {
        background-image: none;
        background-color: alpha(currentColor, 0.10);
        font-size: 16px;
        border: 1px solid alpha(currentColor, 0.20);
        min-width: 22px;
        min-height: 160px;
        padding: 0;
        box-shadow: none;
        opacity: 0;
        transition: opacity 160ms ease;
    }
    .drawer-tab       { border-left: none;  border-radius: 0 10px 10px 0; }
    .drawer-tab-right { border-right: none; border-radius: 10px 0 0 10px; }
    .drawer-tab:hover, .drawer-tab-right:hover {
        background-color: alpha(currentColor, 0.20);
        opacity: 1;
    }

    /* The panel the revealer slides in. Opaque via the theme's .background
       class (C++ adds it); here only the edge and the shadow on the open side. */
    .drawer-panel { padding: 12px; }
    .drawer-panel.left  { border-right: 1px solid alpha(currentColor, 0.15);
                          box-shadow: 2px 0 10px alpha(black, 0.25); }
    .drawer-panel.right { border-left: 1px solid alpha(currentColor, 0.15);
                          box-shadow: -2px 0 10px alpha(black, 0.25); }

    /* The dice: no button chrome -- the drawing is the die. */
    button.die { padding: 2px; background: none; box-shadow: none; }
    button.die:focus-visible { outline: 2px solid alpha(currentColor, 0.5); }

    /* Scorecard: the boxes you could take now, and whose turn it is. */
    .card-cell, button.score-option { min-height: 28px; }
    button.score-option { min-width: 48px; padding: 0 8px; font-weight: bold; }
    button.score-option.zero { font-weight: normal; opacity: 0.6; }
    .current-player { border-bottom: 3px solid alpha(currentColor, 0.6); }

    /* Transparent click-away catcher; shown only while a drawer is open. */
    .drawer-scrim { background-color: transparent; }
)CSS";

}  // namespace yatze
