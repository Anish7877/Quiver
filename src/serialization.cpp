#include "serialization.hpp"

auto Serialization::serialize(flatbuffers::FlatBufferBuilder& builder, const ContainerType& obj) -> flatbuffers::Offset<Types::Container> {
        std::vector<flatbuffers::Offset<Types::PairString>> vols_offset{};
        std::vector<flatbuffers::Offset<Types::PairInt32>> ports_offset{};
        for (const auto& vol : obj.volumes) {
                vols_offset.emplace_back(Types::CreatePairString(builder,
                                        builder.CreateString(vol.first),
                                        builder.CreateString(vol.second)));
        }
        for (const auto& port : obj.ports) {
                ports_offset.emplace_back(Types::CreatePairInt32(builder,
                                        port.first, port.second));
        }

        auto vols_vec_offset{builder.CreateVector(vols_offset)};
        auto ports_vec_offset{builder.CreateVector(ports_offset)};
        auto devs_vec_offset{builder.CreateVectorOfStrings(obj.devices)};

        auto id_off{builder.CreateString(obj.id)};
        auto name_off{builder.CreateString(obj.name)};
        auto image_off{builder.CreateString(obj.image)};
        auto status_off{builder.CreateString(obj.status)};
        auto created_at_off{builder.CreateString(obj.created_at)};
        auto hostname_off{builder.CreateString(obj.hostname)};
        auto fs_path_off{builder.CreateString(obj.filesystem_path)};
        auto vfs_path_off{builder.CreateString(obj.vfs_path)};

        Types::ContainerBuilder fb_builder{builder};

        fb_builder.add_pid(obj.pid);
        fb_builder.add_net_pid(obj.net_pid);
        fb_builder.add_vfs(obj.vfs);
        fb_builder.add_no_remove(obj.no_remove);
        fb_builder.add_id(id_off);
        fb_builder.add_name(name_off);
        fb_builder.add_image(image_off);
        fb_builder.add_status(status_off);
        fb_builder.add_created_at(created_at_off);
        fb_builder.add_hostname(hostname_off);
        fb_builder.add_filesystem_path(fs_path_off);
        fb_builder.add_vfs_path(vfs_path_off);
        fb_builder.add_volumes(vols_vec_offset);
        fb_builder.add_devices(devs_vec_offset);
        fb_builder.add_ports(ports_vec_offset);

        return fb_builder.Finish();
}

auto Serialization::serialize(flatbuffers::FlatBufferBuilder& builder, const VolumeType& obj) -> flatbuffers::Offset<Types::Volume> {
        std::vector<flatbuffers::Offset<Types::PairString>> vols_offset{};
        for (const auto& vol : obj.volumes) {
                vols_offset.emplace_back(Types::CreatePairString(builder,
                                        builder.CreateString(vol.first),
                                        builder.CreateString(vol.second)));
        }
        auto id_offset{builder.CreateString(obj.container_id)};
        auto created_at_offset{builder.CreateString(obj.created_at)};
        auto vols_vec_offset{builder.CreateVector(vols_offset)};
        Types::VolumeBuilder fb_builder{builder};
        fb_builder.add_container_id(id_offset);
        fb_builder.add_created_at(created_at_offset);
        fb_builder.add_paths(vols_vec_offset);
        return fb_builder.Finish();
}

auto Serialization::serialize(flatbuffers::FlatBufferBuilder& builder, const DeviceType& obj) -> flatbuffers::Offset<Types::Device> {
        auto id_offset{builder.CreateString(obj.container_id)};
        auto created_at_offset{builder.CreateString(obj.created_at)};
        auto devs_vec_offset{builder.CreateVectorOfStrings(obj.devices)};
        Types::DeviceBuilder fb_builder{builder};
        fb_builder.add_container_id(id_offset);
        fb_builder.add_created_at(created_at_offset);
        fb_builder.add_paths(devs_vec_offset);
        return fb_builder.Finish();
}

auto Serialization::serialize(flatbuffers::FlatBufferBuilder& builder, const NetworkType& obj) -> flatbuffers::Offset<Types::Network> {
        std::vector<flatbuffers::Offset<Types::PairInt32>> ports_offset{};
        for (const auto& port : obj.ports) {
                ports_offset.emplace_back(Types::CreatePairInt32(builder,
                                        port.first, port.second));
        }
        auto id_offset{builder.CreateString(obj.container_id)};
        auto created_at_offset{builder.CreateString(obj.created_at)};
        auto ports_vec_offset{builder.CreateVector(ports_offset)};
        Types::NetworkBuilder fb_builder{builder};
        fb_builder.add_container_id(id_offset);
        fb_builder.add_created_at(created_at_offset);
        fb_builder.add_ports(ports_vec_offset);
        return fb_builder.Finish();
}

auto Serialization::serialize(flatbuffers::FlatBufferBuilder& builder, const ImageType& obj) -> flatbuffers::Offset<Types::Image> {
        auto id_offset{builder.CreateString(obj.id)};
        auto name_offset{builder.CreateString(obj.name)};
        auto tag_offset{builder.CreateString(obj.tag)};
        auto path_offset{builder.CreateString(obj.path)};
        auto created_at_offset{builder.CreateString(obj.created_at)};
        Types::ImageBuilder fb_builder{builder};
        return fb_builder.Finish();
}

auto Serialization::deserialize(const Types::Container* fb) -> ContainerType {
        ContainerType obj{};
        if(!fb) return obj;

        obj.pid = fb->pid();
        obj.net_pid = fb->net_pid();
        obj.vfs = fb->vfs();
        obj.no_remove = fb->no_remove();

        if (fb->id()) obj.id = fb->id()->str();
        if (fb->name()) obj.name = fb->name()->str();
        if (fb->image()) obj.image = fb->image()->str();
        if (fb->status()) obj.status = fb->status()->str();
        if (fb->created_at()) obj.created_at = fb->created_at()->str();
        if (fb->hostname()) obj.hostname = fb->hostname()->str();
        if (fb->filesystem_path()) obj.filesystem_path = fb->filesystem_path()->str();
        if (fb->vfs_path()) obj.vfs_path = fb->vfs_path()->str();

        if (fb->volumes()) {
                for (const auto* vol : *fb->volumes()) {
                        obj.volumes.emplace_back(std::make_pair(vol->host_path() ? vol->host_path()->str() : "",
                                                                vol->container_path() ? vol->container_path()->str() : ""));
                }
        }

        if (fb->ports()) {
                for (const auto* port : *fb->ports()) {
                        obj.ports.push_back({port->host_port(), port->container_port()});
                }
        }

        if (fb->devices()) {
                for (const auto* dev : *fb->devices()) {
                        obj.devices.push_back(dev->str());
                }
        }

        return obj;
}

auto Serialization::deserialize(const Types::Volume* fb) -> VolumeType {
        VolumeType obj{};
        if (!fb) return obj;
        if (fb->container_id()) {obj.container_id =  fb->container_id()->str();}
        if (fb->created_at()) {obj.created_at = fb->created_at()->str();}
        if (fb->paths()) {
                for (const auto* vol : *fb->paths()) {
                        obj.volumes.emplace_back(std::make_pair(vol->host_path() ? vol->host_path()->str() : "",
                                                                vol->container_path() ? vol->container_path()->str(): ""));
                }
        }
        return obj;
}

auto Serialization::deserialize(const Types::Device* fb) -> DeviceType {
        DeviceType obj{};
        if (!fb) return obj;
        if (fb->container_id()) obj.container_id = fb->container_id()->str();
        if (fb->created_at()) obj.created_at = fb->created_at()->str();
        if (fb->paths()) {
                for (const auto* dev : *fb->paths()) {
                        obj.devices.emplace_back(dev->str());
                }
        }
        return obj;
}

auto Serialization::deserialize(const Types::Network* fb) -> NetworkType {
        NetworkType obj{};
        if (!fb) return obj;
        if (fb->container_id()) obj.container_id = fb->container_id()->str();
        if (fb->created_at()) obj.created_at = fb->created_at()->str();
        if (fb->ports()) {
                for(const auto* port : *fb->ports()) {
                        obj.ports.emplace_back(std::make_pair(port->host_port(), port->container_port()));
                }
        }
        return obj;
}

auto Serialization::deserialize(const Types::Image* fb) -> ImageType {
        ImageType obj{};
        if (!fb) return obj;
        if (fb->id()) obj.id = fb->id()->str();
        if (fb->name()) obj.name = fb->name()->str();
        if (fb->tag()) obj.tag = fb->tag()->str();
        if (fb->path()) obj.path = fb->path()->str();
        if (fb->created_at()) obj.created_at = fb->created_at()->str();
        return obj;
}
