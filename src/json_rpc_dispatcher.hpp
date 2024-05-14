#ifndef JSON_RPC_DISPATCHER_HPP
#define JSON_RPC_DISPATCHER_HPP

#include <string>
#include <unordered_map>
#include <functional>
#include <tuple>
#include <type_traits>
#include <nlohmann/json.hpp>
#include <cstdint>

namespace jrcpd {

// Local implementation of index_sequence and make_index_sequence,
// for esp32 toolchain compatibility
template<std::size_t... Is>
struct index_sequence {};

template<std::size_t N, std::size_t... Is>
struct make_index_sequence : make_index_sequence<N-1, N-1, Is...> {};

template<std::size_t... Is>
struct make_index_sequence<0, Is...> : index_sequence<Is...> {};

template<typename F, typename... Args>
struct is_callable {
    template<typename U> static auto test(U* p) -> decltype((*p)(std::declval<Args>()...), void(), std::true_type());
    template<typename U> static auto test(...) -> decltype(std::false_type());

    static constexpr bool value = decltype(test<F>(0))::value;
};

// Convert JSON to tuple
template<typename... Args, std::size_t... Is>
std::tuple<Args...> json_to_tuple_impl(const nlohmann::json& j, index_sequence<Is...>) {
    return std::make_tuple(j.at(Is).template get<typename std::decay<Args>::type>()...);
}

template<typename... Args>
std::tuple<Args...> json_to_tuple(const nlohmann::json& j) {
    return json_to_tuple_impl<Args...>(j, make_index_sequence<sizeof...(Args)>{});
}

// Invoke function with tuple arguments
template<typename Func, typename Tuple, std::size_t... Is>
auto invoke_impl(Func func, Tuple& args, index_sequence<Is...>) -> decltype(func(std::get<Is>(args)...)) {
    return func(std::get<Is>(args)...);
}

template<typename Func, typename Tuple>
auto invoke(Func func, Tuple& args) -> decltype(invoke_impl(func, args, make_index_sequence<std::tuple_size<Tuple>::value>{})) {
    return invoke_impl(func, args, make_index_sequence<std::tuple_size<Tuple>::value>{});
}

// Extract argument types from function
template<typename Func>
struct function_traits;

template<typename Ret, typename... Args>
struct function_traits<Ret(*)(Args...)> {
    using return_type = Ret;
    using argument_types = std::tuple<Args...>;
};

template<typename Ret, typename... Args>
struct function_traits<std::function<Ret(Args...)>> {
    using return_type = Ret;
    using argument_types = std::tuple<Args...>;
};

// Helper to check if any of the argument types are references
template<typename... Args>
struct contains_reference;

template<>
struct contains_reference<> : std::false_type {};

template<typename First, typename... Rest>
struct contains_reference<First, Rest...> {
    static const bool value = std::is_reference<First>::value || contains_reference<Rest...>::value;
};

// Helper to generate JSON response
template<typename T>
std::string generate_response(bool status, const T& result) {
    nlohmann::json response = {{"ok", status}, {"result", result}};
    return response.dump();
}

// List of allowed types for method arguments and return values
// We support only flat types for RPC calls to keep things simple
using AllowedTypes = std::tuple<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float, double, std::string, bool>;

// Helper to check if a type is in a list of allowed types
template<typename T, typename... Allowed>
struct is_allowed_type : std::false_type {};

template<typename T, typename First, typename... Rest>
struct is_allowed_type<T, First, Rest...> : std::conditional<std::is_same<T, First>::value, std::true_type, is_allowed_type<T, Rest...>>::type {};

template<typename T>
struct is_allowed_type<T> : std::false_type {};

template<typename T>
struct is_allowed_type<T, AllowedTypes> {
    static const bool value = is_allowed_type<T, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float, double, std::string, bool>::value;
};

} // namespace jrcpd

class JsonRpcDispatcher {
public:
    JsonRpcDispatcher() = default;

    // For functions with arguments
    template<typename Func, typename std::enable_if<!jrcpd::is_callable<Func>::value, int>::type = 0>
    void addMethod(const std::string& name, Func func) {
        using traits = jrcpd::function_traits<decltype(func)>;
        using Ret = typename traits::return_type;
        using ArgsTuple = typename traits::argument_types;

        static_assert(!std::is_same<Ret, void>::value, "Return type must be JSON-compatible");
        static_assert(!jrcpd::contains_reference<typename std::tuple_element<0, ArgsTuple>::type, typename std::tuple_element<1, ArgsTuple>::type>::value, "Arguments cannot be references");

        // Ensure the return type and argument types are from the allowed types list
        static_assert(jrcpd::is_allowed_type<Ret, jrcpd::AllowedTypes>::value, "Return type is not allowed");
        static_assert(jrcpd::is_allowed_type<typename std::tuple_element<0, ArgsTuple>::type, jrcpd::AllowedTypes>::value, "Argument type is not allowed");

        functions[name] = [func](const nlohmann::json& args) -> std::string {
            try {
                auto tpl_args = jrcpd::json_to_tuple<typename std::tuple_element<0, ArgsTuple>::type, typename std::tuple_element<1, ArgsTuple>::type>(args);
                Ret result = jrcpd::invoke(func, tpl_args);
                return jrcpd::generate_response(true, result);
            } catch (const nlohmann::json::type_error&) {
                return jrcpd::generate_response(false, "Argument type mismatch");
            } catch (const std::exception& e) {
                return jrcpd::generate_response(false, e.what());
            }
        };
    }

    // For functions without arguments
    template<typename Func, typename std::enable_if<jrcpd::is_callable<Func>::value, int>::type = 0>
    void addMethod(const std::string& name, Func func) {
        using traits = jrcpd::function_traits<decltype(func)>;
        using Ret = typename traits::return_type;

        static_assert(jrcpd::is_allowed_type<Ret, jrcpd::AllowedTypes>::value, "Return type is not allowed");

        functions[name] = [func](const nlohmann::json& /*args*/) -> std::string {
            try {
                Ret result = func();
                return jrcpd::generate_response(true, result);
            } catch (const std::exception& e) {
                return jrcpd::generate_response(false, e.what());
            }
        };
    }

    std::string dispatch(const std::string& input) {
        try {
            nlohmann::json request = nlohmann::json::parse(input);
            std::string method = request.at("method");
            nlohmann::json args = request.at("args");

            auto it = functions.find(method);
            if (it != functions.end()) {
                std::string result = it->second(args);
                return result;
            } else {
                return jrcpd::generate_response(false, "Method not found");
            }
        } catch (const std::exception& e) {
            return jrcpd::generate_response(false, e.what());
        }
    }

private:
    std::unordered_map<std::string, std::function<std::string(const nlohmann::json&)>> functions;
};

#endif // JSON_RPC_DISPATCHER_HPP
