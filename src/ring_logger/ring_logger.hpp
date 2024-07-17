#pragma once

#include <vector>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <iostream>
#include "ring_logger_helpers.hpp"
#include "ring_logger_buffer.hpp"
#include "ring_logger_packer.hpp"
#include "ring_logger_formatter.hpp"

enum class RingLoggerLevel {
    DEBUG,
    INFO,
    ERROR,
    NONE // Highest level to disable all logs
};

namespace ring_logger {

    template<RingLoggerLevel level, const char* label, RingLoggerLevel CompileTimeLogLevel, const char* allowedLabels, const char* ignoredLabels>
    struct should_log {
        static const bool value = level >= CompileTimeLogLevel &&
                                  (std::strcmp(allowedLabels, "") == 0 || std::strcmp(allowedLabels, "*") == 0 || is_label_in_list(label, allowedLabels)) &&
                                  !is_label_in_list(label, ignoredLabels);
    };

} // namespace ring_logger

template<
    size_t BufferSize = 10 * 1024,
    RingLoggerLevel CompileTimeLogLevel = RingLoggerLevel::DEBUG,
    size_t MaxRecordSize = 512,
    size_t MaxArgs = 10,
    const char* AllowedLabels = ring_logger::EMPTY_STRING,
    const char* IgnoredLabels = ring_logger::EMPTY_STRING
>
class RingLogger {
public:
    RingLogger() : packer(), ringBuffer() {}

    template<RingLoggerLevel level, typename... Args>
    void push(const char* message, const Args&... msgArgs) {
        lpush<level, ring_logger::EMPTY_STRING>(message, msgArgs...);
    }

    template<RingLoggerLevel level, const char* label, typename... Args>
    typename std::enable_if<ring_logger::should_log<level, label, CompileTimeLogLevel, AllowedLabels, IgnoredLabels>::value>::type
    lpush(const char* message, const Args&... msgArgs) {
        static_assert(ring_logger::are_supported_types<typename std::decay<Args>::type...>::value, "Unsupported argument type");
        static_assert(level != RingLoggerLevel::NONE, "NONE log level is invalid for logging");
        static_assert(label[0] != ' ', "Label should not start with a space");
        static_assert(label[0] == '\0' || label[std::strlen(label) - 1] != ' ', "Label should not end with a space");

        uint32_t timestamp = 0;
        uint8_t level_as_byte = static_cast<uint8_t>(level);
        size_t packedSize = packer.getPackedSize(timestamp, level_as_byte, label, message, msgArgs...);

        if (packedSize > MaxRecordSize) {
            auto packedData = packer.pack(timestamp, level_as_byte, label, "[TOO BIG]");
            ringBuffer.writeRecord(packedData.data, packedData.size);
            return;
        }

        auto packedData = packer.pack(timestamp, level_as_byte, label, message, msgArgs...);
        ringBuffer.writeRecord(packedData.data, packedData.size);
    }

    template<RingLoggerLevel level, const char* label, typename... Args>
    typename std::enable_if<!ring_logger::should_log<level, label, CompileTimeLogLevel, AllowedLabels, IgnoredLabels>::value>::type
    lpush(const char* /*message*/, const Args&... /*msgArgs*/) {
        // Empty implementation for disabled conditions
    }

    bool pull(char* outputBuffer, size_t bufferSize) {
        uint8_t recordData[MaxRecordSize];
        size_t recordSize = MaxRecordSize;

        if (!ringBuffer.readRecord(recordData, recordSize)) {
            return false; // No records available
        }

        typename ring_logger::Packer<MaxRecordSize, MaxArgs>::UnpackedData unpackedData;
        typename ring_logger::Packer<MaxRecordSize, MaxArgs>::PackedData packedData = {{0}, 0};
        std::memcpy(packedData.data, recordData, recordSize);
        packedData.size = recordSize;

        if (!packer.unpack(packedData, unpackedData)) {
            return false; // Failed to unpack
        }

        uint32_t timestamp = unpackedData.data[0].uint32Value;
        uint8_t level_as_byte = unpackedData.data[1].uint8Value;
        RingLoggerLevel level = static_cast<RingLoggerLevel>(level_as_byte);
        const char* label = unpackedData.data[2].stringValue;
        const char* message = unpackedData.data[3].stringValue;

        // Write log header
        size_t offset = writeLogHeader(outputBuffer, bufferSize, timestamp, level, label);

        // Format and write message with args
        ring_logger::Formatter::print(outputBuffer + offset, bufferSize - offset, message, unpackedData.data + 4, unpackedData.size - 4);

        return std::strlen(outputBuffer);
    }

    // Forwarding to push with INFO level
    template<typename... Args>
    void push_info(const char* message, const Args&... msgArgs) {
        push<RingLoggerLevel::INFO>(message, msgArgs...);
    }

    // Forwarding to lpush with INFO level
    template<const char* label, typename... Args>
    void lpush_info(const char* message, const Args&... msgArgs) {
        lpush<RingLoggerLevel::INFO, label>(message, msgArgs...);
    }

    // Forwarding to push with DEBUG level
    template<typename... Args>
    void push_debug(const char* message, const Args&... msgArgs) {
        push<RingLoggerLevel::DEBUG>(message, msgArgs...);
    }

    // Forwarding to lpush with DEBUG level
    template<const char* label, typename... Args>
    void lpush_debug(const char* message, const Args&... msgArgs) {
        lpush<RingLoggerLevel::DEBUG, label>(message, msgArgs...);
    }

    // Forwarding to push with ERROR level
    template<typename... Args>
    void push_error(const char* message, const Args&... msgArgs) {
        push<RingLoggerLevel::ERROR>(message, msgArgs...);
    }

    // Forwarding to lpush with ERROR level
    template<const char* label, typename... Args>
    void lpush_error(const char* message, const Args&... msgArgs) {
        lpush<RingLoggerLevel::ERROR, label>(message, msgArgs...);
    }

private:
    ring_logger::Packer<MaxRecordSize, MaxArgs> packer;
    ring_logger::RingBuffer<BufferSize> ringBuffer;

    size_t writeLogHeader(char* outputBuffer, size_t bufferSize, uint32_t /*timestamp*/, RingLoggerLevel level, const char* label) {
        using namespace ring_logger;

        const char* levelStr = nullptr;
        switch (level) {
            case RingLoggerLevel::DEBUG: levelStr = "DEBUG"; break;
            case RingLoggerLevel::INFO: levelStr = "INFO"; break;
            case RingLoggerLevel::ERROR: levelStr = "ERROR"; break;
            case RingLoggerLevel::NONE: levelStr = "NONE"; break;
            default: levelStr = "UNKNOWN"; break;
        }

        if (label[0] == '\0') {
            Formatter::print(outputBuffer, bufferSize, "[{}]: ", ArgVariant(levelStr));
        } else {
            Formatter::print(outputBuffer, bufferSize, "[{}] [{}]: ", ArgVariant(levelStr), ArgVariant(label));
        }

        return std::strlen(outputBuffer);
    }
};
