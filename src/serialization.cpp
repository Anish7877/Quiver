#include "serialization.hpp"

auto Serialization::serialize(flatbuffers::FlatBufferBuilder&, const ContainerType&) -> flatbuffers::Offset<Types::Container> {
        flatbuffers::Offset<Types::Container> container_record{};
        return container_record;
}

auto Serialization::serialize(flatbuffers::FlatBufferBuilder&, const VolumeType&) -> flatbuffers::Offset<Types::Volume> {
        flatbuffers::Offset<Types::Volume> ret;
        return ret;
}

auto Serialization::serialize(flatbuffers::FlatBufferBuilder&, const DeviceType&) -> flatbuffers::Offset<Types::Device> {
        flatbuffers::Offset<Types::Device> ret;
        return ret;
}

auto Serialization::serialize(flatbuffers::FlatBufferBuilder&, const NetworkType&) -> flatbuffers::Offset<Types::Network> {
        flatbuffers::Offset<Types::Network> ret;
        return ret;
}

auto Serialization::serialize(flatbuffers::FlatBufferBuilder&, const ImageType&) -> flatbuffers::Offset<Types::Image> {
        flatbuffers::Offset<Types::Image> ret;
        return ret;
}

auto Serialization::deserialize(const Types::Container*) -> ContainerType {
        ContainerType ret;
        return ret;
}

auto Serialization::deserialize(const Types::Volume*) -> VolumeType {
        VolumeType ret;
        return ret;
}

auto Serialization::deserialize(const Types::Device*) -> DeviceType {
        DeviceType ret;
        return ret;
}

auto Serialization::deserialize(const Types::Network*) -> NetworkType {
        NetworkType ret;
        return ret;
}

auto Serialization::deserialize(const Types::Image*) -> ImageType {
        ImageType image{};
        return image;
}
