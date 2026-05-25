#include "tests.hpp"
#include "utils.hpp"
#include "serialization.hpp"
#include <flatbuffers/flatbuffers.h>

auto Tests::test_utils() -> void {
        test(Utils::dir_exists, true, "/home");
        test(Utils::file_exists, true, "/home/anish/Desktop/a.cpp");
        test(Utils::ensure_dir, "/home/anish/Desktop/test_dir", 0755);
        test(Utils::ensure_file, "/home/anish/Desktop/test_dir/a.txt");
        test(Utils::write_file, "/home/anish/Desktop/test_dir/a.txt", "Hello World", false);
        test(Utils::get_base_dir, "/home/anish/.quiver");
        test(Utils::get_image_path, "/home/anish/.quiver/images/abc", "abc");
}

auto Tests::test_serialization() -> void
{
        ContainerConfig c{};
        c.container_id              = "quiver_test_1";
        c.hostname                  = "test-host";
        c.pid                       = 1234;
        c.vfs                       = true;
        c.terminal.value            = true;
        c.detach.value              = false;
        c.console_size.height       = 24;
        c.console_size.width        = 80;
        c.rootfs.path               = "/var/lib/containers/rootfs";
        c.rootfs.read_only          = false;
        c.rootfs_propagation.type   = "private";
        c.cwd.value                 = "/home/user";
        c.env.value                 = {"PATH=/usr/bin", "HOME=/root"};
        c.args.value                = {"/bin/bash"};
        c.no_new_privileges.value   = true;
        c.oom_score.value           = -500;
        c.devices                   = {{ "/dev/null", "/dev/null" }, { "/dev/zero", "/dev/zero" }};
        c.mounts                    = {{ {}, {}, {}, "/proc", "proc", "proc" }};
        c.networks.tcp_ports        = {"8080:80"};
        c.networks.auto_tcp         = true;

        auto serialize_and_verify{[](const ContainerConfig& obj) -> bool {
                flatbuffers::FlatBufferBuilder builder{};
                auto offset{Serialization::serialize(builder, obj)};
                builder.Finish(offset);
                flatbuffers::Verifier verifier{builder.GetBufferPointer(), builder.GetSize()};
                return verifier.VerifyBuffer<FB::ContainerConfig>(nullptr);
        }};

        test(serialize_and_verify, true, c);

        ImageMetadata img{};
        img.id           = "sha256:abc123";
        img.name         = "ubuntu";
        img.tag          = "22.04";
        img.digest       = "sha256:abc123def456";
        img.path         = "/var/lib/images/ubuntu_22_04";
        img.size_bytes   = 72 * 1024 * 1024;
        img.created_at   = 1700000000;
        img.architecture = "amd64";
        img.source       = "docker.io/library/ubuntu";

        auto serialize_and_verify_img{[](const ImageMetadata& obj) -> bool {
                flatbuffers::FlatBufferBuilder builder{};
                auto offset{Serialization::serialize(builder, obj)};
                builder.Finish(offset);
                flatbuffers::Verifier verifier{builder.GetBufferPointer(), builder.GetSize()};
                return verifier.VerifyBuffer<FB::ImageMetadata>(nullptr);
        }};

        test(serialize_and_verify_img, true, img);
}

auto Tests::test_deserialization() -> void
{
        ContainerConfig original{};
        original.container_id             = "quiver_test_2";
        original.hostname                 = "db-host";
        original.domain_name              = "internal.local";
        original.pid                      = 9000;
        original.net_pid                  = 9001;
        original.vfs                      = false;
        original.terminal.value           = false;
        original.detach.value             = true;
        original.console_size.height      = 48;
        original.console_size.width       = 160;
        original.rootfs.path              = "/var/lib/containers/db_rootfs";
        original.rootfs.read_only         = true;
        original.rootfs_propagation.type  = "slave";
        original.cwd.value                = "/var/lib/postgresql";
        original.env.value                = {"PGDATA=/var/lib/postgresql/data", "POSTGRES_USER=admin"};
        original.args.value               = {"postgres", "-c", "max_connections=200"};
        original.oom_score.value          = -1000;
        original.no_new_privileges.value  = true;
        original.user.uid                 = 999;
        original.user.gid                 = 999;
        original.uid_mapping              = {0, 1000, 65536};
        original.gid_mapping              = {0, 1000, 65536};
        original.capabilities.bounding    = {"CAP_NET_BIND_SERVICE"};
        original.devices                  = {{"/dev/tty", "/dev/tty"}};
        original.networks.tcp_ports       = {"5432:5432"};
        original.networks.auto_tcp        = false;
        original.mounts                   = {{ {}, {"ro"}, {}, "/etc/postgresql", "bind", "/host/pgconf" }};
        original.namespaces               = {{"/proc/9000/ns/pid", "pid"}, {"/proc/9000/ns/net", "network"}};
        original.masked_paths.paths       = {"/proc/kcore", "/proc/sysrq-trigger"};
        original.read_only_paths.paths    = {"/proc/asound", "/proc/bus"};

        auto roundtrip{[](const ContainerConfig& obj) -> ContainerConfig {
                flatbuffers::FlatBufferBuilder builder{};
                auto offset{Serialization::serialize(builder, obj)};
                builder.Finish(offset);
                const auto* fb_root{flatbuffers::GetRoot<FB::ContainerConfig>(builder.GetBufferPointer())};
                return Serialization::deserialize(fb_root);
        }};

        ContainerConfig restored{roundtrip(original)};

        test([](const ContainerConfig& c) { return c.container_id; },
             std::string("quiver_test_2"), restored);

        test([](const ContainerConfig& c) { return c.pid; },
             static_cast<pid_t>(9000), restored);

        test([](const ContainerConfig& c) { return c.rootfs.read_only; },
             true, restored);

        test([](const ContainerConfig& c) { return c.cwd.value; },
             std::string("/var/lib/postgresql"), restored);

        test([](const ContainerConfig& c) { return c.env.value.size(); },
             std::size_t{2}, restored);

        test([](const ContainerConfig& c) { return c.env.value.at(0); },
             std::string("PGDATA=/var/lib/postgresql/data"), restored);

        test([](const ContainerConfig& c) { return c.env.value.at(1); },
             std::string("POSTGRES_USER=admin"), restored);

        test([](const ContainerConfig& c) { return c.user.uid; },
             static_cast<uid_t>(999), restored);

        test([](const ContainerConfig& c) { return c.no_new_privileges.value; },
             true, restored);

        test([](const ContainerConfig& c) { return c.oom_score.value; },
             static_cast<int32_t>(-1000), restored);

        test([](const ContainerConfig& c) { return c.masked_paths.paths.size(); },
             std::size_t{2}, restored);

        ImageMetadata img_original{};
        img_original.id           = "sha256:deadbeef";
        img_original.name         = "alpine";
        img_original.tag          = "3.18";
        img_original.digest       = "sha256:deadbeefcafe";
        img_original.path         = "/var/lib/images/alpine_3_18";
        img_original.size_bytes   = 8 * 1024 * 1024;
        img_original.created_at   = 1710000000;
        img_original.architecture = "arm64";
        img_original.source       = "docker.io/library/alpine";

        auto img_roundtrip{[](const ImageMetadata& obj) -> ImageMetadata {
                flatbuffers::FlatBufferBuilder builder{};
                auto offset{Serialization::serialize(builder, obj)};
                builder.Finish(offset);
                const auto* fb_root{flatbuffers::GetRoot<FB::ImageMetadata>(builder.GetBufferPointer())};
                return Serialization::deserialize(fb_root);
        }};

        ImageMetadata img_restored{img_roundtrip(img_original)};

        test([](const ImageMetadata& m) { return m.id; },
             std::string("sha256:deadbeef"), img_restored);

        test([](const ImageMetadata& m) { return m.tag; },
             std::string("3.18"), img_restored);

        test([](const ImageMetadata& m) { return m.size_bytes; },
             static_cast<uint64_t>(8 * 1024 * 1024), img_restored);

        test([](const ImageMetadata& m) { return m.created_at; },
             static_cast<int64_t>(1710000000), img_restored);

        test([](const ImageMetadata& m) { return m.architecture; },
             std::string("arm64"), img_restored);
}
