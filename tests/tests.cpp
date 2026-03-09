#include "tests.hpp"
#include "utils.hpp"
#include "logger.hpp"
#include <flatbuffers/flatbuffers.h>
#include "container_type_generated.h"
#include "serialization.hpp"

auto Tests::test_utils() -> void {
        test(Utils::dir_exists, true, "/home");
        test(Utils::file_exists, true, "/home/anish/Desktop/a.cpp");
        test(Utils::ensure_dir, "/home/anish/Desktop/test_dir", 0755);
        test(Utils::ensure_file, "/home/anish/Desktop/test_dir/a.txt");
        test(Utils::write_file, "/home/anish/Desktop/test_dir/a.txt", "Hello World", false);
        test(Utils::get_base_dir, "/home/anish/.quiver");
        test(Utils::get_sock_path, "/home/anish/.quiver/containers/123", 123);
        test(Utils::get_filesystem_path, "/home/anish/.quiver/filesystems/123", 123);
        test(Utils::get_vfs_path, "/home/anish/.quiver/vfs/123", 123);
        test(Utils::get_image_path, "/home/anish/.quiver/images/abc", "abc");
        test(Utils::get_logs_path, "/home/anish/.quiver/logs/123", 123);
}

auto Tests::test_logger() -> void {
        using namespace std::chrono_literals;
        Logger logger{};
        auto set_log_path_func{&Logger::set_log_path};
        auto log_func{&Logger::log};
        test(set_log_path_func, logger, fs::path("/home/anish/Desktop/test_log.log"));
        for(int i{0}; i<10;++i){
                std::string msg{std::to_string(i)};
                test(log_func, logger, msg);
                std::this_thread::sleep_for(500ms);
        }
}

auto Tests::test_serialization() -> void {
        ContainerType c{};
        c.id = "quiver_test_1";
        c.pid = 1234;
        c.image = "ubuntu:22.04";
        c.vfs = true;
        c.devices = {"/dev/null", "/dev/zero"};
        c.volumes = {{"/host/data", "/container/data"}};

        auto serialize_and_verify{[](const ContainerType& obj) -> bool {
                flatbuffers::FlatBufferBuilder builder{};
                auto offset{Serialization::serialize(builder, obj)};
                builder.Finish(offset);
                flatbuffers::Verifier verifier(builder.GetBufferPointer(), builder.GetSize());
                return verifier.VerifyBuffer<Types::Container>(nullptr);
        }};
        test(serialize_and_verify, true, c);
}

auto Tests::test_deserialization() -> void {
        ContainerType original{};
        original.id = "quiver_test_2";
        original.pid = 9000;
        original.name = "my_database_container";
        original.devices = {"/dev/tty"};
        original.volumes = {{"/tmp", "/var/tmp"}};
        auto serialize_deserialize{[](const ContainerType& obj) -> ContainerType {
                flatbuffers::FlatBufferBuilder builder{};
                auto offset{Serialization::serialize(builder, obj)};
                builder.Finish(offset);
                const auto* fb_root{flatbuffers::GetRoot<Types::Container>(builder.GetBufferPointer())};
                return Serialization::deserialize(fb_root);
        }};
        ContainerType restored{serialize_deserialize(original)};
        auto check_id{[](const ContainerType& c) { return c.id; }};
        test(check_id, std::string("quiver_test_2"), restored);
        auto check_pid{[](const ContainerType& c) { return c.pid; }};
        test(check_pid, 9000u, restored);
}
