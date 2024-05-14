#include <iostream>
#include <string>
#include "json_rpc_dispatcher.hpp"

using json = nlohmann::json;

// Example functions
int add(int a, int b) {
    return a + b;
}

std::string concat(std::string a, std::string b) {
    return a + b;
}

// Main function
int main() {
    JsonRpcDispatcher dispatcher;

    // Add methods
    dispatcher.addMethod("add", add);
    dispatcher.addMethod("concat", concat);

    // Example usage
    std::string input = R"({"method": "add", "args": [1, 2]})";
    std::string result = dispatcher.dispatch(input);
    std::cout << result << std::endl;

    input = R"({"method": "concat", "args": ["hello ", "world"]})";
    result = dispatcher.dispatch(input);
    std::cout << result << std::endl;

    input = R"({"method": "unknown", "args": []})";
    result = dispatcher.dispatch(input);
    std::cout << result << std::endl;

    return 0;
}
