#include "tests.hpp"
#include "utils.hpp"

auto main() -> int {
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
        test(Utils::remove_directory_recursively, true, "/home/anish/Desktop/test_dir/a.txt");
        test(Utils::print_usage);
        return 0;
}
