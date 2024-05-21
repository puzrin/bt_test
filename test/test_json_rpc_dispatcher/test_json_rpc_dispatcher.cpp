#include <unity.h>

// Undefine isnan and isinf to avoid conflict with nlohmann/json.hpp
#undef isnan
#undef isinf

#include "json_rpc_dispatcher.hpp"

// Example functions for testing
int8_t add_8bits(int8_t a, int8_t b) { return a + b; }

std::string concat(std::string a, std::string b) { return a + b; }

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

/*
// Test string data method, when passed by reference
std::string concat_by_ref(const std::string& a, const std::string& b) {
    std::string result = a + b;
    return result;
}

void test_string_by_ref_data() {
    JsonRpcDispatcher dispatcher;
    dispatcher.addMethod("concat", concat_by_ref);

    std::string input = R"({"method": "concat", "args": ["hello ", "world"]})";
    std::string expected = R"({"ok":true,"result":"hello world"})";
    std::string result = dispatcher.dispatch(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}
*/

// Test unknown method
void test_unknown_method() {
    JsonRpcDispatcher dispatcher;

    std::string input = R"({"method": "unknown", "args": []})";
    std::string expected = R"({"ok":false,"result":"Method not found"})";
    std::string result = dispatcher.dispatch(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

// Test bad input with no method
void test_no_method_prop() {
    JsonRpcDispatcher dispatcher;

    std::string input = R"({"args": []})";
    std::string expected = R"({"ok":false,"result":"Method not found"})";
    std::string result = dispatcher.dispatch(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

// Test bad input with wrong method prop type
void test_method_prop_wrong_type() {
    JsonRpcDispatcher dispatcher;

    std::string input = R"({"method": [], "args": []})";
    std::string expected = R"({"ok":false,"result":"Method not found"})";
    std::string result = dispatcher.dispatch(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

// Test bad input with no args
void test_no_args_prop() {
    JsonRpcDispatcher dispatcher;
    dispatcher.addMethod("add_8bits", add_8bits);

    std::string input = R"({"method": "add_8bits"})";
    std::string expected = R"({"ok":false,"result":"Number of arguments mismatch"})";
    std::string result = dispatcher.dispatch(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

// Test bad input with wrong args prop type
void test_args_prop_wrong_type() {
    JsonRpcDispatcher dispatcher;
    dispatcher.addMethod("add_8bits", add_8bits);

    std::string input = R"({"method": "add_8bits", "args": 5})";
    std::string expected = R"({"ok":false,"result":"Number of arguments mismatch"})";
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

// Test no parameters
int noargs() { return 5; }

void test_no_args() {
    JsonRpcDispatcher dispatcher;
    dispatcher.addMethod("noparams", noargs);

    std::string input = R"({"method": "noparams", "args": []})";
    std::string expected = R"({"ok":true,"result":5})";
    std::string result = dispatcher.dispatch(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

int throw_exception() {
    throw std::runtime_error("Test exception");
}

// Test method throws exception
void test_method_throws_exception() {
    JsonRpcDispatcher dispatcher;
    dispatcher.addMethod("throw_exception", throw_exception);

    std::string input = R"({"method": "throw_exception", "args": []})";
    std::string expected = R"({"ok":false,"result":"Test exception"})";
    std::string result = dispatcher.dispatch(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

int one_argument(int a) {
    return a * 2;
}

void test_one_argument() {
    JsonRpcDispatcher dispatcher;
    dispatcher.addMethod("one_argument", one_argument);

    std::string input = R"({"method": "one_argument", "args": [2]})";
    std::string expected = R"({"ok":true,"result":4})";
    std::string result = dispatcher.dispatch(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

int three_arguments(int a, int b, int c) {
    return a + b + c;
}

void test_three_arguments() {
    JsonRpcDispatcher dispatcher;
    dispatcher.addMethod("three_arguments", three_arguments);

    std::string input = R"({"method": "three_arguments", "args": [1, 2, 3]})";
    std::string expected = R"({"ok":true,"result":6})";
    std::string result = dispatcher.dispatch(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

// Test broken JSON input
void test_broken_json_input() {
    JsonRpcDispatcher dispatcher;
    dispatcher.addMethod("add_8bits", add_8bits);

    std::string input = R"({"method": "add_8bits", "args": [1, 2)"; // Missing closing brace
    std::string expected = R"({"ok":false,"result":"IncompleteInput"})";
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
    RUN_TEST(test_no_method_prop);
    RUN_TEST(test_method_prop_wrong_type);
    RUN_TEST(test_no_args_prop);
    RUN_TEST(test_args_prop_wrong_type);
    RUN_TEST(test_args_overflow);
    RUN_TEST(test_add_wrong_arg_type_float);
    RUN_TEST(test_add_wrong_arg_type_string);
    RUN_TEST(test_add_wrong_arg_type_null);
    RUN_TEST(test_concat_wrong_arg_type_int);
    RUN_TEST(test_concat_wrong_arg_type_null);
    RUN_TEST(test_no_args);
    RUN_TEST(test_method_throws_exception);
    RUN_TEST(test_one_argument);
    RUN_TEST(test_three_arguments);
    RUN_TEST(test_broken_json_input);
    return UNITY_END();
}
