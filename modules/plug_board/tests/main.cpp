#include <gtest/gtest.h>
#include <iostream>

// GoogleTest 1.16 does not provide the newer fail-if-no-tests option. Make the
// contract explicit so an accidental empty/filter-only traversal cannot pass CI.
int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();
    if (testing::UnitTest::GetInstance()->test_to_run_count() == 0) {
        std::cerr << "Plug Board contract validation ran no tests.\n";
        return 2;
    }
    return result;
}
