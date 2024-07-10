#include <gtest/gtest.h>
#include "ring_logger/ring_logger.hpp"

static constexpr char label[] = "foo";

TEST(RingLoggerTest, ShowTemporaryOutput) {
    RingLogger<> logger;

    logger.push_info("Hello, World!", 1, 2, 3, "foo bar");
    logger.push<RingLoggerLevel::ERROR>("Hello, World!", 1, 2, 3);

    logger.lpush_info<label>("Hello!", 4, 5);
}

// Main function to run the tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
