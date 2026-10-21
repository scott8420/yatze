#include "Engine.hpp"
#include "Log.hpp"

#include <glibmm/miscutils.h>

#include <algorithm>
#include <chrono>
#include <filesystem>

namespace yatze {

Engine::Engine() : m_table(std::make_unique<model::strategy::Table>()) {
    m_done.connect(sigc::mem_fun(*this, &Engine::on_built));
}

Engine::~Engine() {
    m_cancel = true;
    if (m_worker.joinable()) m_worker.join();
}

std::string Engine::cache_file() const {
    return Glib::build_filename(Glib::get_user_cache_dir(), "yatze", "strategy-v1.bin");
}

void Engine::start() {
    const auto file = cache_file();
    if (m_table->load(file)) {
        m_progress = 1.0f;
        if (auto lg = log::get(log::Area::Game)) lg->info("strategy: loaded {} ({:.4f})", file, m_table->start());
        m_ready.emit();
        return;
    }
    if (auto lg = log::get(log::Area::Game)) lg->info("strategy: no cache -- building");
    m_building = std::make_unique<model::strategy::Table>();
    m_worker = std::thread([this] {
        const auto t0 = std::chrono::steady_clock::now();
        // Leave a core for the UI.
        const int threads = std::max(1, int(std::thread::hardware_concurrency()) - 1);
        if (!m_building->build(threads, &m_cancel, &m_progress)) return;   // cancelled
        if (auto lg = log::get(log::Area::Game))
            lg->info("strategy: built in {:.1f} s",
                     std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
        m_done.emit();
    });
}

void Engine::on_built() {
    if (m_worker.joinable()) m_worker.join();
    if (!m_building || !m_building->ready()) return;
    m_table = std::move(m_building);
    std::error_code ec;
    const auto file = cache_file();
    std::filesystem::create_directories(std::filesystem::path(file).parent_path(), ec);
    const bool saved = m_table->save(file);
    if (auto lg = log::get(log::Area::Game))
        lg->info("strategy: ready ({:.4f}); cache {} {}", m_table->start(), saved ? "saved to" : "NOT saved to", file);
    m_ready.emit();
}

}  // namespace yatze
