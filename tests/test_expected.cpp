#include "../src/expected.h"
#include <cstdio>
#include <string>

using namespace pluto;

int main() {
    // Test Ok
    auto r1 = Ok<int>(42);
    if (!r1.ok()) { std::fprintf(stderr, "FAIL: Ok should be ok\n"); return 1; }
    if (r1.value() != 42) { std::fprintf(stderr, "FAIL: Ok value should be 42\n"); return 1; }
    
    // Test Err
    auto r2 = Err<int>(std::string("error message"));
    if (r2.ok()) { std::fprintf(stderr, "FAIL: Err should not be ok\n"); return 1; }
    if (r2.error() != "error message") { std::fprintf(stderr, "FAIL: Err error should match\n"); return 1; }
    
    // Test map
    auto r3 = Ok<int>(10).map([](int x) { return x * 2; });
    if (!r3.ok() || r3.value() != 20) { std::fprintf(stderr, "FAIL: map failed\n"); return 1; }
    
    auto r4 = Err<int>(std::string("err")).map([](int x) { return x * 2; });
    if (r4.ok()) { std::fprintf(stderr, "FAIL: map on Err should propagate error\n"); return 1; }
    
    // Test and_then
    auto r5 = Ok<int>(5).and_then([](int x) -> Result<int> { return Ok(x + 1); });
    if (!r5.ok() || r5.value() != 6) { std::fprintf(stderr, "FAIL: and_then failed\n"); return 1; }
    
    auto r6 = Err<int>(std::string("err")).and_then([](int x) -> Result<int> { return Ok(x + 1); });
    if (r6.ok()) { std::fprintf(stderr, "FAIL: and_then on Err should propagate error\n"); return 1; }
    
    // Test or_else
    auto r7 = Err<int>(std::string("err")).or_else([](const std::string&) -> Result<int> { return Ok(99); });
    if (!r7.ok() || r7.value() != 99) { std::fprintf(stderr, "FAIL: or_else failed\n"); return 1; }
    
    // Test void Result (using empty type for success)
    auto r8 = Ok<int>(0); // Use int with dummy value for void-like
    if (!r8.ok()) { std::fprintf(stderr, "FAIL: Ok should be ok\n"); return 1; }
    
    auto r9 = Err<int>(std::string("void error"));
    if (r9.ok()) { std::fprintf(stderr, "FAIL: Err should not be ok\n"); return 1; }
    
    std::printf("All tests passed!\n");
    return 0;
}
