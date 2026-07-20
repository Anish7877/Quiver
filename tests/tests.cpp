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
        test(Utils::get_image_path, "/home/anish/.quiver/images/myregistry.com_library_ubuntu_22.04", "myregistry.com/library/ubuntu:22.04");

        // Test OCI whiteouts
        auto test_whiteouts{[]() {
                fs::path temp_dir{"/tmp/quiver_test_whiteouts"};
                Utils::remove_directory(temp_dir);
                Utils::ensure_dir(temp_dir);
                Utils::ensure_dir(temp_dir / "a");
                Utils::write_file(temp_dir / "a" / "b", "hello", false);
                Utils::write_file(temp_dir / "a" / "c", "world", false);
                
                // Create tarball with .wh.b
                fs::path upper_dir{"/tmp/quiver_test_whiteouts_upper"};
                Utils::remove_directory(upper_dir);
                Utils::ensure_dir(upper_dir);
                Utils::ensure_dir(upper_dir / "a");
                Utils::write_file(upper_dir / "a" / ".wh.b", "", false);
                Utils::write_file(upper_dir / "a" / ".wh..wh..opq", "", false);
                
                fs::path tar_path{"/tmp/quiver_test_whiteouts.tar.gz"};
                Utils::create_tar_gz(upper_dir.string(), tar_path.string());
                
                Utils::extract_oci_layer(tar_path.string(), temp_dir.string());
                
                if (fs::exists(temp_dir / "a" / "b") || fs::exists(temp_dir / "a" / "c")) {
                        throw std::runtime_error("Whiteout extraction failed");
                }
                if (fs::exists(temp_dir / "a" / ".wh.b") || fs::exists(temp_dir / "a" / ".wh..wh..opq")) {
                        throw std::runtime_error("Whiteout markers should not be extracted");
                }
        }};
        test(test_whiteouts);

        // Test UID/GID preservation in create_oci_layer
        auto test_oci_layer_ownership{[]() {
                fs::path temp_dir{"/tmp/quiver_test_ownership"};
                Utils::remove_directory(temp_dir);
                Utils::ensure_dir(temp_dir);
                Utils::write_file(temp_dir / "test_file", "hello", false);
                
                // We'll just test that whatever ownership it has is preserved.
                // It currently has our uid/gid.
                struct stat st;
                stat((temp_dir / "test_file").c_str(), &st);
                uid_t expected_uid = st.st_uid;
                gid_t expected_gid = st.st_gid;
                
                fs::path tar_path{"/tmp/quiver_test_ownership.tar.gz"};
                Utils::create_oci_layer(temp_dir.string(), tar_path.string());
                
                // Read the tar archive to verify
                struct archive* a = archive_read_new();
                archive_read_support_format_all(a);
                archive_read_support_filter_all(a);
                if (archive_read_open_filename(a, tar_path.c_str(), 10240) != ARCHIVE_OK) {
                        throw std::runtime_error("Failed to open tar for reading");
                }
                struct archive_entry* entry;
                bool found = false;
                while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
                        std::string path = archive_entry_pathname(entry);
                        if (path == "test_file") {
                                found = true;
                                if (archive_entry_uid(entry) != expected_uid || archive_entry_gid(entry) != expected_gid) {
                                        throw std::runtime_error("UID/GID not preserved");
                                }
                        }
                }
                archive_read_free(a);
                if (!found) throw std::runtime_error("File not found in archive");
        }};
        test(test_oci_layer_ownership);
        auto test_resolve_user_group{[]() {
                fs::path temp_dir{"/tmp/quiver_test_resolve_user"};
                Utils::remove_directory(temp_dir);
                Utils::ensure_dir(temp_dir / "etc");
                
                Utils::write_file(temp_dir / "etc" / "passwd", "root:x:0:0:root:/root:/bin/bash\ntestuser:x:1001:1002::/home/testuser:/bin/sh\n", false);
                Utils::write_file(temp_dir / "etc" / "group", "root:x:0:\ntestgroup:x:1002:\n", false);
                
                std::vector<std::string> lower_dirs{temp_dir.string()};
                
                auto [uid1, gid1] = Utils::resolve_user_group(lower_dirs, "1005:1006");
                if (uid1 != 1005 || gid1 != 1006) throw std::runtime_error("numeric uid/gid failed");
                
                auto [uid2, gid2] = Utils::resolve_user_group(lower_dirs, "testuser:testgroup");
                if (uid2 != 1001 || gid2 != 1002) throw std::runtime_error("named user/group failed");
                
                auto [uid3, gid3] = Utils::resolve_user_group(lower_dirs, "testuser");
                if (uid3 != 1001 || gid3 != 1002) throw std::runtime_error("named user failed");
                
                Utils::remove_directory(temp_dir);
        }};
        test(test_resolve_user_group);
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
