#pragma once
#include "job_processor.hpp"
#include "singleton.hpp"
#include "types.hpp"
#include <rocksdb/db.h>

class DatabaseJobProcessor : public JobProcessor<DatabaseJobData>, public Singleton<DatabaseJobProcessor> {
        friend class Singleton<DatabaseJobProcessor>;
        private:
                DatabaseJobProcessor() = default;
                ~DatabaseJobProcessor() = default;
        public:
                DatabaseJobProcessor(const DatabaseJobProcessor&) = delete;
                DatabaseJobProcessor(DatabaseJobProcessor&&) = delete;
                auto operator=(const DatabaseJobProcessor&) -> DatabaseJobProcessor& = delete;
                auto operator=(DatabaseJobProcessor&&) -> DatabaseJobProcessor& = delete;

                auto init() -> void override;
                auto process_job() -> void override;
        private:
                auto route_job(const DatabaseJobData&) -> void override;
                auto process_log(const rocksdb::DB* const, const DatabaseJobData&) -> void;
};
