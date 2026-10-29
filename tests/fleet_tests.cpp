#include "../src/modules/individual/variant_tests.hpp"
#include <iostream>
int main() { int failed=0; for(const auto& t : FleetTests::run()) {
 std::cout << (t.second ? "PASS " : "FAIL ") << t.first << "\n"; if(!t.second) ++failed;
} return failed ? 1 : 0; }
