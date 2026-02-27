#pragma once

template<typename T>
class Singleton {
        protected:
                Singleton() = default;
                ~Singleton() = default;
        public:
                Singleton(const Singleton&) = delete;
                Singleton(Singleton&&) = delete;
                auto operator=(const Singleton&) -> Singleton& = delete;
                auto operator=(Singleton&&) -> Singleton& = delete;
                static auto get_instance() noexcept -> T& {
                        static T instance{};
                        return instance;
                }
};
