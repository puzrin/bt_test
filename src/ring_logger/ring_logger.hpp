#pragma once

#include <vector>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <iostream>

#include "ring_logger_helpers.hpp"

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

// Helper function to print arguments (for demonstration purposes)
template<typename T>
void print_arg(const T& arg) {
    std::cout << arg << " ";
}

// RingLogger Class Definition
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
    template<RingLoggerLevel level, typename... Args>
    static void push(const char* message, const Args&... msgArgs) {
        lpush<level, ring_logger::EMPTY_STRING>(message, msgArgs...);
    }

    template<RingLoggerLevel level, const char* label, typename... Args>
    static typename std::enable_if<ring_logger::should_log<level, label, CompileTimeLogLevel, AllowedLabels, IgnoredLabels>::value>::type
    lpush(const char* message, const Args&... msgArgs) {
        static_assert(ring_logger::are_supported_types<typename std::decay<Args>::type...>::value, "Unsupported argument type");
        static_assert(level != RingLoggerLevel::NONE, "NONE log level is invalid for logging");
        static_assert(label[0] != ' ', "Label should not start with a space");
        static_assert(label[0] == '\0' || label[std::strlen(label) - 1] != ' ', "Label should not end with a space");

        // Temporary print of all parameters
        std::cout << "Level: " << static_cast<int>(level) << ", Label: " << label << ", Message: " << message << ", Args: ";
        (void)std::initializer_list<int>{(print_arg(msgArgs), 0)...};
        std::cout << std::endl;

        // Implement labeled push functionality
    }

    template<RingLoggerLevel level, const char* label, typename... Args>
    static typename std::enable_if<!ring_logger::should_log<level, label, CompileTimeLogLevel, AllowedLabels, IgnoredLabels>::value>::type
    lpush(const char* message, const Args&... msgArgs) {
        // Empty implementation for disabled conditions
    }

    static bool pull(char* outputBuffer, size_t bufferSize) {
        // Implement pull functionality
        return false; // Placeholder return value
    }

    // Forwarding to push with INFO level
    template<typename... Args>
    static void push_info(const char* message, const Args&... msgArgs) {
        push<RingLoggerLevel::INFO>(message, msgArgs...);
    }

    // Forwarding to lpush with INFO level
    template<const char* label, typename... Args>
    static void lpush_info(const char* message, const Args&... msgArgs) {
        lpush<RingLoggerLevel::INFO, label>(message, msgArgs...);
    }
};
