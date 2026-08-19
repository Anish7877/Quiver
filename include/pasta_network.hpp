#pragma once
#include "oci_runtime.hpp"

class PastaNetwork {
        public:
                PastaNetwork() = default;
                ~PastaNetwork() = default;
                PastaNetwork(const PastaNetwork&) = delete;
                PastaNetwork(PastaNetwork&&) = delete;
                auto operator=(const PastaNetwork&) -> PastaNetwork& = delete;
                auto operator=(PastaNetwork&&) -> PastaNetwork& = delete;

                static auto setup_networking(pid_t, const OCIRuntime::Network&) -> pid_t;
};
