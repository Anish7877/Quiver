#pragma once
#include "oci_runtime.hpp"

class Schedular {
        public:
                Schedular() = default;
                ~Schedular() = default;
                Schedular(const Schedular&) = delete;
                Schedular(Schedular&&) = delete;
                auto operator=(const Schedular&) -> Schedular& = delete;
                auto operator=(Schedular&&) -> Schedular& = delete;

                static auto apply_opts(const OCIRuntime::SchedularOpts&) -> void;
};
