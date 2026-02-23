#include "include/Backend.h"

namespace Quiver {


struct Backend::BackendImpl {
    std::vector<Container> containers_ {
        {"7f8a1b", "nginx-proxy", "nginx:alpine", "running"},
        {"3c4d5e", "redis-cache", "redis:6.2", "stopped"},
        {"9a0b1c", "postgres-db", "postgres:14", "running"}
    };
};

Backend::Backend() : pimpl_{std::make_unique<BackendImpl>()} {}

Backend::~Backend() = default;

auto Backend::get_instance() -> Backend& {
    static Backend instance {};
    return instance;
}

auto Backend::get_containers() const -> std::vector<Container> {
    return pimpl_->containers_;
}

auto Backend::add_container(const Container& container) -> void {
    pimpl_->containers_.push_back(container);
}

auto Backend::delete_container(const QString& container_id) -> void {
    auto& containers_ref { pimpl_->containers_ };

    containers_ref.erase(
        std::remove_if(containers_ref.begin(), containers_ref.end(),
                       [&container_id](const Container& c) { return c.id == container_id; }),
        containers_ref.end()
        );
}

}
