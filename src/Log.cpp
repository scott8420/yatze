#include "Log.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>

#include <array>
#include <cstdlib>
#include <string>

namespace yatze::log {

namespace {
constexpr int kAreaCount = 5;

const char* area_name(Area a) {
    switch (a) {
        case Area::App:      return "app";
        case Area::Shell:    return "shell";
        case Area::Registry: return "registry";
        case Area::Game:     return "game";
        case Area::Io:       return "io";
    }
    return "?";
}

std::array<std::shared_ptr<spdlog::logger>, kAreaCount>& loggers() {
    static std::array<std::shared_ptr<spdlog::logger>, kAreaCount> a{};
    return a;
}
}  // namespace

void init() {
    auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto& a = loggers();
    for (int i = 0; i < kAreaCount; ++i) {
        const auto area = static_cast<Area>(i);
        auto lg = std::make_shared<spdlog::logger>(area_name(area), sink);
        // Default INFO. YATZE_DEBUG=1 raises every area to DEBUG (the phase
        // changes of a roll, etc.) -- the "drop to log mode" switch.
        lg->set_level(std::getenv("YATZE_DEBUG") ? spdlog::level::debug : spdlog::level::info);
        a[static_cast<std::size_t>(i)] = lg;
    }
}

std::shared_ptr<spdlog::logger> get(Area area) {
    return loggers()[static_cast<std::size_t>(area)];
}

void set_level(Area area, spdlog::level::level_enum level) {
    if (auto lg = get(area)) lg->set_level(level);
}

}  // namespace yatze::log
