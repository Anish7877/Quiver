#pragma once
#include "types.hpp"
#include "serialization.hpp"

template<typename T>
requires FlatbufferSerializable<T> && FlatbufferDeserializable<T>
class DatabaseManager {
        public:
                DatabaseManager() = default;
                virtual ~DatabaseManager() = default;
                DatabaseManager(const DatabaseManager&) = delete;
                DatabaseManager(DatabaseManager&&) = delete;
                auto operator=(const DatabaseManager&) = delete;
                auto operator=(DatabaseManager&&) = delete;
                virtual auto init() -> void = 0;
                virtual auto process_job(const DatabaseJobData&, const T&, Status&) -> void = 0;
        private:
                virtual auto process_get_job(const DatabaseJobData&, Status&) -> void = 0;
                virtual auto process_put_job(const DatabaseJobData&, const T&, Status&) -> void = 0;
                virtual auto process_update_job(const DatabaseJobData&, const T&, Status&) -> void = 0;
                virtual auto process_delete_job(const DatabaseJobData&, Status&) -> void = 0;
};
