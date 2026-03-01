#pragma once
#include <string>

namespace JsonSerialization {
        auto serialize_job_data() -> std::string;
        auto deserialize_job_data() -> void;
}
