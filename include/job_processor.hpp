#pragma once

template<typename T>
class JobProcessor {
        public:
                JobProcessor() = default;
                virtual ~JobProcessor() = default;
                JobProcessor(const JobProcessor&) = delete;
                JobProcessor(JobProcessor&&) = delete;
                auto operator=(const JobProcessor&) -> JobProcessor& = delete;
                auto operator=(JobProcessor&&) -> JobProcessor& = delete;

                virtual auto init() -> void = 0;
                virtual auto process_job() -> void = 0;
        private:
                virtual auto route_job(const T&) -> void = 0;
};
