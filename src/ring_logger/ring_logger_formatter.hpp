#pragma once

#include <cstddef>
#include <cstring>
#include <string>
#include <array>
#include <type_traits>
#include "ring_logger_helpers.hpp"

namespace ring_logger {

struct FormatPlaceholder { const char* start; const char* end; };

// Helper struct to check if a single type is ArgVariant
template<typename T>
struct is_arg_variant : std::is_same<typename std::decay<T>::type, ArgVariant> {};

// Helper struct to check if all types in a parameter pack are ArgVariant
template<typename... Args>
struct all_are_arg_variant;

template<>
struct all_are_arg_variant<> : std::true_type {};

template<typename T, typename... Rest>
struct all_are_arg_variant<T, Rest...>
    : std::integral_constant<bool, is_arg_variant<T>::value && all_are_arg_variant<Rest...>::value> {};

class Formatter {
public:
    bool print(char* output, std::size_t max_length, const char* message, const ArgVariant* args, std::size_t num_args) const {
        if (!output || !message || max_length == 0) return false;

        std::size_t out_index = 0;
        std::size_t arg_index = 0;

        while (*message && out_index < max_length) {
            FormatPlaceholder placeholder = get_next_placeholder(message);

            if (placeholder.start && arg_index < num_args) {
                if (!copy(message, output, out_index, placeholder.start - message, max_length)) return false;
                message = placeholder.end;

                if (!write(output, out_index, max_length, args[arg_index++])) return false;
            } else {
                if (!copy(message, output, out_index, std::strlen(message), max_length)) return false;
                break;
            }
        }

        write_trailing_zero(output, max_length, out_index);
        return (out_index < max_length) ? true : false;
    }

    // Variadic template print function constrained to accept only ArgVariant types
    template<typename... Args>
    typename std::enable_if<all_are_arg_variant<Args...>::value, bool>::type
    print(char* output, std::size_t max_length, const char* message, Args&&... args) const {
        std::array<ArgVariant, sizeof...(args)> arg_array = { std::forward<Args>(args)... };
        return print(output, max_length, message, arg_array.data(), arg_array.size());
    }

private:
    FormatPlaceholder get_next_placeholder(const char* str) const {
        while (*str) 
            if (*str == '{' && *(str + 1) == '}') return {str, str + 2}; else ++str;
        return {nullptr, nullptr};
    }

    bool write(char* output, std::size_t& out_index, std::size_t max_length, const ArgVariant& arg) const {
        std::string arg_str;
        switch (arg.type) {
            case ArgTypeTag::INT8: arg_str = std::to_string(arg.int8Value); break;
            case ArgTypeTag::INT16: arg_str = std::to_string(arg.int16Value); break;
            case ArgTypeTag::INT32: arg_str = std::to_string(arg.int32Value); break;
            case ArgTypeTag::UINT8: arg_str = std::to_string(arg.uint8Value); break;
            case ArgTypeTag::UINT16: arg_str = std::to_string(arg.uint16Value); break;
            case ArgTypeTag::UINT32: arg_str = std::to_string(arg.uint32Value); break;
            case ArgTypeTag::STRING:
                return copy(arg.stringValue ? arg.stringValue : "", output, out_index, std::strlen(arg.stringValue ? arg.stringValue : ""), max_length);
            default:
                return false;
        }
        return copy(arg_str.c_str(), output, out_index, arg_str.size(), max_length);
    }

    bool copy(const char* src, char* dest, std::size_t& out_index, std::size_t size, std::size_t max_allowed) const {
        if (out_index + size > max_allowed) return false;
        std::memcpy(dest + out_index, src, size);
        out_index += size;
        return true;
    }

    void write_trailing_zero(char* output, std::size_t max_length, std::size_t out_index) const {
        if (out_index < max_length) output[out_index] = '\0';
        else output[max_length - 1] = '\0';
    }
};

} // namespace ring_logger
