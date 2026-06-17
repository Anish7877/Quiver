#pragma once

#include "container_config_generated.h"
#include "container_config.hpp"
#include "image_metadata_generated.h"
#include "layer_cache_generated.h"
#include "types.hpp"

#include <concepts>
#include <type_traits>
#include <flatbuffers/buffer.h>
#include <flatbuffers/flatbuffer_builder.h>

namespace Serialization {

        auto serialize  (flatbuffers::FlatBufferBuilder&, const ContainerConfig&) -> flatbuffers::Offset<FB::ContainerConfig>;
        auto deserialize(const FB::ContainerConfig*) -> ContainerConfig;

        auto serialize  (flatbuffers::FlatBufferBuilder&, const ImageMetadata&) -> flatbuffers::Offset<FB::ImageMetadata>;
        auto deserialize(const FB::ImageMetadata*) -> ImageMetadata;

        auto serialize (flatbuffers::FlatBufferBuilder&, const LayerCache&) -> flatbuffers::Offset<FB::LayerCache>;
        auto deserialize(const FB::LayerCache*) -> LayerCache;
}

template<typename T>
struct FlatbufferTraits;

template<> struct FlatbufferTraits<ContainerConfig> { using FbRootType = FB::ContainerConfig; };
template<> struct FlatbufferTraits<ImageMetadata>   { using FbRootType = FB::ImageMetadata; };
template<> struct FlatbufferTraits<LayerCache>     { using FbRootType = FB::LayerCache; };

template<typename T>
struct is_flatbuffer_offset : std::false_type {};

template<typename U>
struct is_flatbuffer_offset<flatbuffers::Offset<U>> : std::true_type {};

template<typename T>
concept is_fb_offset = is_flatbuffer_offset<T>::value;

template<typename T>
concept FlatbufferSerializable = requires (const T& obj, flatbuffers::FlatBufferBuilder& builder) {
                { Serialization::serialize(builder, obj) } -> is_fb_offset;
        };

template<typename T>
concept FlatbufferDeserializable = requires (const typename FlatbufferTraits<T>::FbRootType* fb_root) {
                { Serialization::deserialize(fb_root) } -> std::same_as<T>;
        };
