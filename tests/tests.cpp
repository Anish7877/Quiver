#include "tests.hpp"
#include "utils.hpp"
#include "monitor.hpp"

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

auto Tests::test_monitor() -> void {
        using namespace std::chrono_literals;
        auto set_log_path_func{&Monitor::set_log_path};
        auto log_func{&Monitor::log};
        test(set_log_path_func, Monitor::get_instance(), fs::path("/home/anish/Desktop/test_log.log"));
        for(int i{0}; i<10;++i){
                std::string msg{std::to_string(i) + "\n"};
                test(log_func, Monitor::get_instance(), msg);
                std::this_thread::sleep_for(1000ms);
        }
}
