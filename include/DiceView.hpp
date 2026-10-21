#pragma once
#include "gfx/Math.hpp"
#include "model/Rules.hpp"

#include <gtkmm/glarea.h>
#include <sigc++/signal.h>

#include <array>
#include <cstdint>

// DiceView -- the dice on a felt table, in OpenGL (s004). Replaces DiceRow
// behind the same surface: set() shows the model, signal_die() asks to hold.
//
// Two ways of looking (Scott, Oct 21; s004b):
//   * ROLLED  -- a seated player's 3/4 view: the dice lie where they fell,
//                results up. The viewpoint is never quite the same twice
//                (steep nose-down to low across the felt, a little lean,
//                closer or farther), seeded per roll.
//   * PRESENTED -- the dice squared up in a row, results ON TOP, the camera
//                looking down about 50 degrees to read them. Held dice sit a
//                little forward on a blue ring; the hint's suggestion glows
//                amber under its dice. Click a die to hold it.
// Nothing runs on a fixed clock: each die lands at its own moment, and the
// pause and the glide vary per roll (s005 makes the whole roll real physics).
// A roll cuts to ROLLED (s004: a short drop; s005 replaces it with the
// recorded take -- physics + a planned camera), then the camera and the dice
// glide into PRESENTED together.
//
// Pips are drawn by the fragment shader from the same face table the selftest
// checks (gfx/DieFaces.hpp) -- no textures, no asset files. The scene renders
// into a 4x multisampled framebuffer and is blitted to the GLArea.
// If OpenGL 3.3 cannot start, signal_failed() fires and the Shell shows the
// flat DiceRow instead.
namespace yatze {

class DiceView : public Gtk::GLArea {
public:
    DiceView();
    ~DiceView() override;

    // The model's dice. `roll_key` changes exactly when new dice were rolled
    // (the Shell passes round / seat / roll), which is what starts the roll
    // sequence; a hold changing only moves that die.
    void set(const model::Dice& dice, const std::array<bool, 5>& held, bool can_hold,
             std::uint64_t roll_key);
    void set_glow(const std::array<bool, 5>& glow);      // the hint's suggested dice
    // Pixels of the view's left edge covered by a drawer (0 = none): the
    // presented row re-centres in what is still visible.
    void set_left_inset(float px);
    void set_red(bool red);                              // red dice / white pips, else ivory / black

    sigc::signal<void(int)>& signal_die() { return m_sig_die; }
    sigc::signal<void()>&    signal_failed() { return m_sig_failed; }
    bool failed() const { return m_failed; }

protected:
    void on_realize() override;
    void on_unrealize() override;
    bool on_render(const Glib::RefPtr<Gdk::GLContext>& ctx) override;

private:
    struct Pose { gfx::Vec3 pos; gfx::Quat rot; };
    struct Cam  { gfx::Vec3 eye, at; };

    bool init_gl();
    void free_gl();
    void ensure_fbo(int w, int h);
    bool tick(const Glib::RefPtr<Gdk::FrameClock>& clock);
    void start_ticking();

    // Layout (world units: a die is 1; the felt is y = 0; you sit toward +Z).
    Pose present_pose(int i) const;
    Cam  present_cam() const;
    void plan_roll(std::uint64_t seed);                  // scatter + the seated viewpoint
    void update_poses(double t, double dt);              // where everything is at time t
    gfx::Mat4 view_proj(const Cam& c, float aspect) const;
    int  pick(double x, double y) const;

    // Model mirror.
    model::Dice          m_dice{};
    std::array<bool, 5>  m_held{}, m_glow{};
    bool                 m_can_hold = false;
    std::uint64_t        m_roll_key = 0;

    // Animation.
    enum class Phase { Present, Drop, Rolled, Glide } m_phase = Phase::Present;
    double               m_phase_t0 = 0, m_now = 0, m_last = -1;
    std::array<Pose, 5>  m_pose{}, m_from{}, m_scatter{};
    Cam                  m_cam{}, m_roll_cam{};
    std::array<float, 5> m_lift{};                        // held dice step forward smoothly
    guint                m_tick_id = 0;
    int                  m_hover = -1;
    bool                 m_red = true;
    float                m_aspect = 1.6f;                 // the view's shape, for fitting the row
    float                m_inset_frac = 0.0f;             // share of the width under a drawer
    // This roll's pace and the presented height (drawn per roll in plan_roll).
    double               m_drop = 0.8, m_rest = 1.4, m_glide = 1.9;
    std::array<float, 5> m_fall{0.8f, 0.8f, 0.8f, 0.8f, 0.8f};
    float                m_present_el = 0.87f;            // ~50 deg above the felt

    // GL objects.
    bool         m_failed = false;
    unsigned     m_die_prog = 0, m_flat_prog = 0;
    unsigned     m_die_vao = 0, m_die_vbo = 0, m_die_ibo = 0, m_quad_vao = 0, m_quad_vbo = 0;
    int          m_die_count = 0;
    unsigned     m_fbo = 0, m_fbo_color = 0, m_fbo_depth = 0;
    int          m_fbo_w = 0, m_fbo_h = 0, m_samples = 4;

    sigc::signal<void(int)> m_sig_die;
    sigc::signal<void()>    m_sig_failed;
};

}  // namespace yatze
