#pragma once
#include "common_header.hpp"
#include <QString>

namespace Quiver {

struct Container {
    QString id {};
    QString name {};
    QString image {};
    QString status {};
};

class Backend {
public:
    Backend();
    ~Backend();

    Backend(const Backend&) = delete;
    Backend(Backend&&) = delete;
    auto operator=(const Backend&) -> Backend& = delete;


    static auto get_instance() -> Backend&;

    auto get_containers() const -> std::vector<Container>;
    auto add_container(const Container& container) -> void;
    auto delete_container(const QString& container_id) -> void;

private:
    struct BackendImpl;

    std::unique_ptr<BackendImpl> pimpl_ {};
};

}
