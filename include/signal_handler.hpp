#pragma once
#include <atomic>

namespace SignalHandler {
        auto setup_signal_handlers(std::atomic<bool>&) -> void;
}
