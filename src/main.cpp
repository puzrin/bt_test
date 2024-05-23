#include <iostream>
#include <string>
#include "json_rpc_dispatcher.hpp"

// Example functions
std::string concat(std::string a, std::string b) { return a + b; }

int main() {
    JsonRpcDispatcher dispatcher;

    // Add methods
    dispatcher.addMethod("add", [](int a, int b) { return a + b; });
    dispatcher.addMethod("concat", concat);

    // Example usage
    std::string input = R"({"method": "add", "args": [1, 2]})";
    std::cout << dispatcher.dispatch(input) << std::endl;

    input = R"({"method": "concat", "args": ["hello ", "world"]})";
    std::cout << dispatcher.dispatch(input) << std::endl;

    input = R"({"method": "unknown", "args": []})";
    std::cout << dispatcher.dispatch(input) << std::endl;

    return 0;
}
