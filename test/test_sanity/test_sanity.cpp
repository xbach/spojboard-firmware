// Harness smoke test.
//
// This suite deliberately asserts something trivial: its job is to prove the
// [env:native] toolchain, Unity and the test runner all work on the desktop, so
// that when a real suite fails you know it is the logic and not the harness.
//
// Real extractions land next to it as pure-logic modules are pulled out of the
// Arduino-dependent code -- RestPolicy first (TA-0268 tier 1a), then the
// DisplayColors wildcard matcher, the platform-symbol map parser and
// countStopIds/getStopIdAt.
#include <unity.h>

void test_sanity(void) { TEST_ASSERT_EQUAL_INT(2, 1 + 1); }

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_sanity);
    return UNITY_END();
}
