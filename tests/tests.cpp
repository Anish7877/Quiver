#include "tests.hpp"
#include "json_serialization.hpp"
#include "utils.hpp"
#include "logger.hpp"
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>

struct Test{
        std::string msg{};
        auto operator==(const Test&) const -> bool = default;
        friend auto operator<<(std::ostream& os, const Test& t) -> std::ostream& {
                return os << "Test{message: " << t.msg << "}";
        }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Test, msg)

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
        Test msg{"hello"};
        std::string expected_string{json(msg).dump()};
        auto serialize_wrapper{[](const auto& val) {
                return JsonSerialization::serialize_data(val);
        }};
        test(serialize_wrapper, expected_string, msg);
}

auto Tests::test_deserialization() -> void {
        Test msg{"hello"};
        std::string json_string{json(msg).dump()};
        auto deserialize_wrapper{[](const std::string& val) {
                return JsonSerialization::deserialize_data<Test>(val);
        }};
        test(deserialize_wrapper, msg, json_string);
}
