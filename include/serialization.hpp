#pragma once
#include "container_type_generated.h"
#include "volume_type_generated.h"
#include "device_type_generated.h"
#include "network_type_generated.h"
#include "image_type_generated.h"
#include "types.hpp"

#include <concepts>
#include <type_traits>
#include <flatbuffers/buffer.h>
#include <flatbuffers/flatbuffer_builder.h>

namespace Serialization {
        auto serialize(flatbuffers::FlatBufferBuilder&, const ContainerType&) -> flatbuffers::Offset<Types::Container>;
        auto serialize(flatbuffers::FlatBufferBuilder&, const VolumeType&) -> flatbuffers::Offset<Types::Volume>;
        auto serialize(flatbuffers::FlatBufferBuilder&, const DeviceType&) -> flatbuffers::Offset<Types::Device>;
        auto serialize(flatbuffers::FlatBufferBuilder&, const NetworkType&) -> flatbuffers::Offset<Types::Network>;
        auto serialize(flatbuffers::FlatBufferBuilder&, const ImageType&) -> flatbuffers::Offset<Types::Image>;

        auto deserialize(const Types::Container*) -> ContainerType;
        auto deserialize(const Types::Volume*) -> VolumeType;
        auto deserialize(const Types::Device*) -> DeviceType;
        auto deserialize(const Types::Network*) -> NetworkType;
        auto deserialize(const Types::Image*) -> ImageType;
}

template<typename T> struct FlatbufferTraits;

template<> struct FlatbufferTraits<ContainerType> { using FbRootType = Types::Container; };
template<> struct FlatbufferTraits<VolumeType>    { using FbRootType = Types::Volume; };
template<> struct FlatbufferTraits<DeviceType>    { using FbRootType = Types::Device; };
template<> struct FlatbufferTraits<NetworkType>   { using FbRootType = Types::Network; };
template<> struct FlatbufferTraits<ImageType>     { using FbRootType = Types::Image; };

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
