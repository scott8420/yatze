#pragma once
#include "model/Strategy.hpp"

#include <glibmm/dispatcher.h>
#include <sigc++/signal.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>

// Engine -- the UI side of the strategy table: load it from the cache, or
// build it on a worker thread (a few seconds, once per machine) and save it.
// The model knows nothing of threads or files' whereabouts; this owns both.
//
//   cache:  $XDG_CACHE_HOME/yatze/strategy-v1.bin   (~4 MB)
//
// The worker touches only its own Table; the Dispatcher hands it to the main
// loop, which swaps it in and emits signal_ready(). Closing the window while
// it builds cancels and joins (no thread outlives the window).
namespace yatze {

class Engine {
public:
    Engine();
    ~Engine();

    void start();                                    // load, else build in the background
    bool  ready() const { return m_table && m_table->ready(); }
    float progress() const { return m_progress.load(); }
    const model::strategy::Table& table() const { return *m_table; }
    sigc::signal<void()>& signal_ready() { return m_ready; }

private:
    std::string cache_file() const;
    void on_built();                                 // main loop: take the worker's table

    std::unique_ptr<model::strategy::Table> m_table;
    std::unique_ptr<model::strategy::Table> m_building;
    std::thread                             m_worker;
    std::atomic<bool>                       m_cancel{false};
    std::atomic<float>                      m_progress{0.0f};
    Glib::Dispatcher                        m_done;
    sigc::signal<void()>                    m_ready;
};

}  // namespace yatze
