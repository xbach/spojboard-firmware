// Which per-tab parsers a config POST may run.
//
// The bug this pins: AP mode renders only the connection and hardware tabs, but
// its save posts no `tab` field, so it arrives as "all". Reading "all" as "every
// parser" then ran parseDisplaySettings() and parseOptionalSettings() against a
// form that contained none of their fields -- and every
// `config->x = server->hasArg("x")` assignment there reads an absent checkbox as
// UNCHECKED. Five settings were silently cleared on every setup save.
#include <unity.h>

#include "../../src/network/TabDispatch.h"
#include "../../src/network/TabDispatch.cpp"

// The ordinary STA case: the page names the panel it submitted.
void test_an_explicitly_named_tab_submits_only_itself(void)
{
    TEST_ASSERT_TRUE(tabSubmitted("display", "display", false));
    TEST_ASSERT_FALSE(tabSubmitted("display", "transit", false));
    TEST_ASSERT_FALSE(tabSubmitted("display", "optional", false));
    TEST_ASSERT_FALSE(tabSubmitted("display", "hardware", false));
}

// Outside AP mode every tab is on the page, so "all" really is all of them.
void test_all_submits_every_tab_when_every_tab_was_rendered(void)
{
    const char* every[] = {"connection", "hardware", "display", "transit", "optional"};
    for (int i = 0; i < 5; i++)
    {
        TEST_ASSERT_TRUE(tabSubmitted("all", every[i], false));
    }
}

// THE REGRESSION GUARD. In AP mode "all" is a claim the page cannot back up:
// DashboardPage renders connection + hardware and nothing else, so display,
// transit and optional posted no fields at all.
void test_all_in_ap_mode_submits_only_the_tabs_ap_mode_renders(void)
{
    TEST_ASSERT_TRUE(tabSubmitted("all", "connection", true));
    TEST_ASSERT_TRUE(tabSubmitted("all", "hardware", true));

    TEST_ASSERT_FALSE(tabSubmitted("all", "display", true));
    TEST_ASSERT_FALSE(tabSubmitted("all", "transit", true));
    TEST_ASSERT_FALSE(tabSubmitted("all", "optional", true));
}

// A named tab is honoured in AP mode too. Nothing posts one today, but the rule
// is "was this tab submitted", and an explicit name is the strongest evidence
// there is -- it must not be overridden by a guess about what AP mode renders.
void test_an_explicit_tab_is_honoured_even_in_ap_mode(void)
{
    TEST_ASSERT_TRUE(tabSubmitted("hardware", "hardware", true));
    TEST_ASSERT_TRUE(tabSubmitted("display", "display", true));
}

// Total function: the one caller cannot pass null (it defaults to "all"), but a
// predicate that decides whether to WRITE config must not be undefined for any
// input it can be handed.
void test_null_arguments_submit_nothing(void)
{
    TEST_ASSERT_FALSE(tabSubmitted(nullptr, "display", false));
    TEST_ASSERT_FALSE(tabSubmitted("all", nullptr, false));
    TEST_ASSERT_FALSE(tabSubmitted(nullptr, nullptr, true));
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_an_explicitly_named_tab_submits_only_itself);
    RUN_TEST(test_all_submits_every_tab_when_every_tab_was_rendered);
    RUN_TEST(test_all_in_ap_mode_submits_only_the_tabs_ap_mode_renders);
    RUN_TEST(test_an_explicit_tab_is_honoured_even_in_ap_mode);
    RUN_TEST(test_null_arguments_submit_nothing);
    return UNITY_END();
}
