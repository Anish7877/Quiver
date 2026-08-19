#pragma once
#include "singleton.hpp"
#include "types.hpp"
#include <optional>

class DatabaseCommandQueue;
class ValueHeap;
class ImageDbManager : public Singleton<ImageDbManager> {
        friend class Singleton<ImageDbManager>;
        private:
                ImageDbManager() = default;
                ~ImageDbManager() = default;
        public:
                ImageDbManager(const ImageDbManager&) = delete;
                ImageDbManager(ImageDbManager&&) = delete;
                auto operator=(const ImageDbManager&) -> ImageDbManager& = delete;
                auto operator=(ImageDbManager&&) -> ImageDbManager& = delete;

                auto init() -> void;
                auto add_image(const ImageMetadata&) -> void;
                auto remove_image(const std::string&) -> void;
                [[nodiscard]] auto get_image(const std::string&) -> std::optional<ImageMetadata>;
                [[nodiscard]] auto get_all_images() -> std::vector<ImageMetadata>;
        private:
                auto extract_metadata(const std::string&) -> std::optional<ImageMetadata>;
                DatabaseCommandQueue* m_db_command_queue{nullptr};
                ValueHeap* m_value_heap{nullptr};
};
