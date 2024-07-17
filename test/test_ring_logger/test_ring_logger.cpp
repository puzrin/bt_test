#include <gtest/gtest.h>
#include "ring_logger/ring_logger.hpp"

// Define test labels
constexpr const char foo_label[] = "foo";
constexpr const char bar_label[] = "bar";
constexpr const char garbage_label[] = "garbage";
constexpr const char whitelisted_labels_list[] = "foo,bar";
constexpr const char ignored_labels_list[] = "garbage";

TEST(RingLoggerTest, BasicPushAndLpush) {
    RingLogger<> logger;
    char buffer[1024] = {0};

    logger.push_info("Hello, {}!", "World");
    ASSERT_TRUE(logger.pull(buffer, sizeof(buffer)));
    EXPECT_STREQ(buffer, "[INFO]: Hello, World!");

    logger.lpush_info<foo_label>("Hello, {}!", "World");
    ASSERT_TRUE(logger.pull(buffer, sizeof(buffer)));
    EXPECT_STREQ(buffer, "[INFO] [foo]: Hello, World!");

    logger.push_debug("Debug message: {}", 123);
    ASSERT_TRUE(logger.pull(buffer, sizeof(buffer)));
    EXPECT_STREQ(buffer, "[DEBUG]: Debug message: 123");

    logger.lpush_debug<bar_label>("Debug message: {}", 123);
    ASSERT_TRUE(logger.pull(buffer, sizeof(buffer)));
    EXPECT_STREQ(buffer, "[DEBUG] [bar]: Debug message: 123");

    logger.push_error("Error message: {}", 456);
    ASSERT_TRUE(logger.pull(buffer, sizeof(buffer)));
    EXPECT_STREQ(buffer, "[ERROR]: Error message: 456");

    logger.lpush_error<foo_label>("Error message: {}", 456);
    ASSERT_TRUE(logger.pull(buffer, sizeof(buffer)));
    EXPECT_STREQ(buffer, "[ERROR] [foo]: Error message: 456");
}

TEST(RingLoggerTest, IgnoreLabel) {
    RingLogger<10 * 1024, RingLoggerLevel::DEBUG, 512, 10, ring_logger::EMPTY_STRING, ignored_labels_list> logger;
    char buffer[1024] = {0};

    logger.lpush_info<garbage_label>("This should be ignored");
    EXPECT_FALSE(logger.pull(buffer, sizeof(buffer)));

    logger.lpush_info<foo_label>("This should be logged");
    ASSERT_TRUE(logger.pull(buffer, sizeof(buffer)));
    EXPECT_STREQ(buffer, "[INFO] [foo]: This should be logged");
}

TEST(RingLoggerTest, WhitelistedLabels) {
    RingLogger<10 * 1024, RingLoggerLevel::DEBUG, 512, 10, whitelisted_labels_list, ring_logger::EMPTY_STRING> logger;
    char buffer[1024] = {0};

    logger.lpush_info<foo_label>("Hello, {}!", "World");
    ASSERT_TRUE(logger.pull(buffer, sizeof(buffer)));
    EXPECT_STREQ(buffer, "[INFO] [foo]: Hello, World!");

    logger.lpush_info<bar_label>("Hello, {}!", "World");
    ASSERT_TRUE(logger.pull(buffer, sizeof(buffer)));
    EXPECT_STREQ(buffer, "[INFO] [bar]: Hello, World!");

    logger.lpush_info<garbage_label>("This should be ignored");
    EXPECT_FALSE(logger.pull(buffer, sizeof(buffer)));
}

/*
TEST(RingLoggerTest, TooBigMessage) {
    RingLogger<> logger;
    char buffer[1024] = {0};
    std::string bigMessage(600, 'A');
    logger.push_info("{}", bigMessage.c_str());
    ASSERT_TRUE(logger.pull(buffer, sizeof(buffer)));
    EXPECT_STREQ(buffer, "[INFO]: [TOO BIG]");
}
*/
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
