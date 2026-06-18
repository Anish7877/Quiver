#pragma once
#include "instruction_types.hpp"
#include "types.hpp"
#include <optional>
#include <memory>

class DatabaseCommandQueue;
class ValueHeap;
class LayerCacheManager {
        public:
                LayerCacheManager() = default;
                ~LayerCacheManager() = default;
                LayerCacheManager(LayerCacheManager&&) = delete;
                LayerCacheManager(const LayerCacheManager&) = delete;
                auto operator=(LayerCacheManager&&) -> LayerCacheManager& = delete;
                auto operator=(const LayerCacheManager&) -> LayerCacheManager& = delete;

                auto init() -> void;
                [[nodiscard]] auto lookup(const std::string&) -> std::optional<LayerCache>;
                [[nodiscard]] auto generate_instruction_hash(const Instruction::InstructionHash&) -> std::string;
                auto store(const std::string&, const std::string&) -> void;
        private:
                std::unique_ptr<DatabaseCommandQueue> m_db_command_queue{};
                std::unique_ptr<ValueHeap> m_value_heap{};
};
