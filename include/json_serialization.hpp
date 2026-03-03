#pragma once
#include <nlohmann/json.hpp>
#include "db_types.hpp"
using json = nlohmann::json;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ContainerType,
                pid,
                net_pid,
                vfs,
                no_remove,
                id,
                name,
                image,
                status,
                created_at,
                hostname,
                filesystem_path,
                pty_shell,
                vfs_path
)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(VolumeType,
                container_id,
                host_container_map
)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ImageType,
                id,
                name,
                tag,
                path,
                created_at
)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NetworkType,
                host_container_map,
                container_id
)

template<typename T>
concept is_serializable = requires(json j, T val) {
        j = val;
};

template<typename T>
concept is_deserializable = requires(json j, T val) {
        val = j.get<T>();
};

namespace JsonSerialization {
        template<typename T>
        requires is_serializable<T>
        auto serialize_data(const T& data) -> std::string {
                json j = data;
                return j.dump();
        }

        template<typename T>
        requires is_deserializable<T>
        auto deserialize_data(const std::string& json_string) -> T {
                json j = json::parse(json_string);
                return j.get<T>();
        }
}
