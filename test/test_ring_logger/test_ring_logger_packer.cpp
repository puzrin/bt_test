#include <gtest/gtest.h>
#include "ring_logger/ring_logger_packer.hpp"

using namespace ring_logger;

constexpr size_t BUFFER_SIZE = 1024;
constexpr size_t ARGUMENTS_COUNT = 10;
using TestPacker = Packer<BUFFER_SIZE, ARGUMENTS_COUNT>;

TEST(PackerTest, PackUnpackIntegers) {
    int8_t int8_val = 42;
    int16_t int16_val = 300;
    int32_t int32_val = 100000;
    uint8_t uint8_val = 255;
    uint16_t uint16_val = 60000;
    uint32_t uint32_val = 4000000000;

    auto packedData = TestPacker::pack(int8_val, int16_val, int32_val, uint8_val, uint16_val, uint32_val);

    TestPacker::UnpackedData unpackedData;
    bool result = TestPacker::unpack(packedData, unpackedData);

    ASSERT_TRUE(result);
    ASSERT_EQ(unpackedData.size, 6u);

    EXPECT_EQ(unpackedData.data[0].type, ArgTypeTag::INT8);
    EXPECT_EQ(unpackedData.data[0].int8Value, int8_val);

    EXPECT_EQ(unpackedData.data[1].type, ArgTypeTag::INT16);
    EXPECT_EQ(unpackedData.data[1].int16Value, int16_val);

    EXPECT_EQ(unpackedData.data[2].type, ArgTypeTag::INT32);
    EXPECT_EQ(unpackedData.data[2].int32Value, int32_val);

    EXPECT_EQ(unpackedData.data[3].type, ArgTypeTag::UINT8);
    EXPECT_EQ(unpackedData.data[3].uint8Value, uint8_val);

    EXPECT_EQ(unpackedData.data[4].type, ArgTypeTag::UINT16);
    EXPECT_EQ(unpackedData.data[4].uint16Value, uint16_val);

    EXPECT_EQ(unpackedData.data[5].type, ArgTypeTag::UINT32);
    EXPECT_EQ(unpackedData.data[5].uint32Value, uint32_val);
}

TEST(PackerTest, PackUnpackString) {
    const char* str_val = "Hello, World!";
    auto packedData = TestPacker::pack(str_val);

    TestPacker::UnpackedData unpackedData;
    bool result = TestPacker::unpack(packedData, unpackedData);

    ASSERT_TRUE(result);
    ASSERT_EQ(unpackedData.size, 1u);

    EXPECT_EQ(unpackedData.data[0].type, ArgTypeTag::STRING);
    EXPECT_STREQ(unpackedData.data[0].stringValue, str_val);
}

TEST(PackerTest, PackUnpackStringLiteral) {
    auto packedData = TestPacker::pack("Hello, World!");

    TestPacker::UnpackedData unpackedData;
    bool result = TestPacker::unpack(packedData, unpackedData);

    ASSERT_TRUE(result);
    ASSERT_EQ(unpackedData.size, 1u);

    EXPECT_EQ(unpackedData.data[0].type, ArgTypeTag::STRING);
    EXPECT_STREQ(unpackedData.data[0].stringValue, "Hello, World!");
}

TEST(PackerTest, GetPackedSize) {
    int8_t int8_val = 42;
    const char* str_val = "Hello, World!";
    size_t packedSize = TestPacker::getPackedSize(int8_val, str_val);

    // Check if the calculated packed size is correct
    ASSERT_EQ(packedSize, 1 + (1 + sizeof(int8_val)) + (1 + 1 + std::strlen(str_val)));
}

TEST(PackerTest, GetPackedSizeWithLiteral) {
    int8_t int8_val = 42;
    size_t packedSize = TestPacker::getPackedSize(int8_val, "Hello, World!");

    // Check if the calculated packed size is correct
    ASSERT_EQ(packedSize, 1 + (1 + sizeof(int8_val)) + (1 + 1 + std::strlen("Hello, World!")));
}

TEST(PackerTest, UnpackUnknownType) {
    TestPacker::PackedData packedData = {};
    packedData.data[0] = 1;  // One argument
    packedData.data[1] = 255; // Invalid type tag
    packedData.size = 2;

    TestPacker::UnpackedData unpackedData;
    bool result = TestPacker::unpack(packedData, unpackedData);

    ASSERT_FALSE(result);
}
