#include "tests.hpp"

auto main() -> int {
        Tests::test_serialization();
        Tests::test_deserialization();
        return 0;
}
