#include <unity.h>

// Undefine isnan and isinf to avoid conflict with nlohmann/json.hpp
#undef isnan
#undef isinf

#include "json_rpc_dispatcher.hpp"

// Example functions for testing
int8_t add_8bits(int8_t a, int8_t b) {
    return a + b;
}

std::string concat(std::string a, std::string b) {
    return a + b;
}

std::string concat_by_ref(const std::string& a, const std::string& b) {
    return a + b;
}

// Test 8bits data method
void test_8bits_data() {
    JsonRpcDispatcher dispatcher;
    dispatcher.addMethod("add_8bits", add_8bits);

    std::string input = R"({"method": "add_8bits", "args": [1, 2]})";
    std::string expected = R"({"ok":true,"result":3})";
    std::string result = dispatcher.dispatch(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

// Test string data method
void test_string_data() {
    JsonRpcDispatcher dispatcher;
    dispatcher.addMethod("concat", concat);

    std::string input = R"({"method": "concat", "args": ["hello ", "world"]})";
    std::string expected = R"({"ok":true,"result":"hello world"})";
    std::string result = dispatcher.dispatch(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

// Test &string data method
/*void test_string_by_ref_data() {
    JsonRpcDispatcher dispatcher;
    dispatcher.addMethod("concat", concat_by_ref);

    std::string input = R"({"method": "concat", "args": ["hello ", "world"]})";
    std::string expected = R"({"ok":true,"result":"hello world"})";
    std::string result = dispatcher.dispatch(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}*/

// Test unknown method
void test_unknown_method() {
    JsonRpcDispatcher dispatcher;

    std::string input = R"({"method": "unknown", "args": []})";
    std::string expected = R"({"ok":false,"result":"Method not found"})";
    std::string result = dispatcher.dispatch(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

// Test args overflow
void test_args_overflow() {
    JsonRpcDispatcher dispatcher;
    dispatcher.addMethod("add_8bits", add_8bits);

    // JSON input with arguments 512 and 512, which are beyond the range of int8_t (-128 to 127).
    std::string input = R"({"method": "add_8bits", "args": [512, 512]})";
    // Current implementation consider that as wrong data type for simplicity.
    std::string expected = R"({"ok":false,"result":"Argument type mismatch"})";
    std::string result = dispatcher.dispatch(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

// Test wrong argument type for add_8bits: float
void test_add_wrong_arg_type_float() {
    JsonRpcDispatcher dispatcher;
    dispatcher.addMethod("add_8bits", add_8bits);

    std::string input = R"({"method": "add_8bits", "args": [1, 2.5]})";
    // Current implementation consider that as wrong data type for simplicity.
    std::string expected = R"({"ok":false,"result":"Argument type mismatch"})";
    std::string result = dispatcher.dispatch(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

// Test wrong argument type for add_8bits: string
void test_add_wrong_arg_type_string() {
    JsonRpcDispatcher dispatcher;
    dispatcher.addMethod("add_8bits", add_8bits);

    std::string input = R"({"method": "add_8bits", "args": [1, "string"]})";
    std::string expected = R"({"ok":false,"result":"Argument type mismatch"})";
    std::string result = dispatcher.dispatch(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

// Test wrong argument type for add_8bits: null
void test_add_wrong_arg_type_null() {
    JsonRpcDispatcher dispatcher;
    dispatcher.addMethod("add_8bits", add_8bits);

    std::string input = R"({"method": "add_8bits", "args": [1, null]})";
    std::string expected = R"({"ok":false,"result":"Argument type mismatch"})";
    std::string result = dispatcher.dispatch(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

// Test wrong argument type for concat: int
void test_concat_wrong_arg_type_int() {
    JsonRpcDispatcher dispatcher;
    dispatcher.addMethod("concat", concat);

    std::string input = R"({"method": "concat", "args": ["hello ", 1]})";
    std::string expected = R"({"ok":false,"result":"Argument type mismatch"})";
    std::string result = dispatcher.dispatch(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

// Test wrong argument type for concat: null
void test_concat_wrong_arg_type_null() {
    JsonRpcDispatcher dispatcher;
    dispatcher.addMethod("concat", concat);

    std::string input = R"({"method": "concat", "args": ["hello ", null]})";
    std::string expected = R"({"ok":false,"result":"Argument type mismatch"})";
    std::string result = dispatcher.dispatch(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

int noargs() {
    return 5;
}

// Test no parameters
void test_no_args() {
    JsonRpcDispatcher dispatcher;
    dispatcher.addMethod("noparams", noargs);

    std::string input = R"({"method": "noparams", "args": []})";
    std::string expected = R"({"ok":true,"result":5})";
    std::string result = dispatcher.dispatch(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

// Setup and teardown functions
void setUp() {}
void tearDown() {}

// Main function to run the tests
int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_8bits_data);
    RUN_TEST(test_string_data);
    // RUN_TEST(test_string_by_ref_data);
    RUN_TEST(test_unknown_method);
    RUN_TEST(test_args_overflow);
    RUN_TEST(test_add_wrong_arg_type_float);
    RUN_TEST(test_add_wrong_arg_type_string);
    RUN_TEST(test_add_wrong_arg_type_null);
    RUN_TEST(test_concat_wrong_arg_type_int);
    RUN_TEST(test_concat_wrong_arg_type_null);
    RUN_TEST(test_no_args);
    return UNITY_END();
}
