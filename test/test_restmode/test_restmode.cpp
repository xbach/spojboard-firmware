// Rest-mode arbitration (TA-0254). Two inputs can turn the panel off: the rest
// SCHEDULE and a human pressing the header button. This suite pins who wins.
//
// The two bugs it exists to prevent, both from one root cause -- a level-triggered
// scheduler that re-asserts its answer every 30 minutes:
//
//   1. The button was DEAD during a scheduled rest. It rendered from
//      (restModeActive && restModeManual), so a scheduled rest drew it inactive,
//      the handler read its own CSS class back as state and posted "enable", and
//      onRestMode()'s guards matched neither branch. A silent no-op with no way to
//      wake the panel short of a power cycle.
//   2. One manual press DISARMED THE SCHEDULE FOREVER. The only way to make an
//      override survive a level-triggered scheduler was to switch the scheduler
//      off, so checkScheduledRestMode() early-returned on restModeManual.
//
// Edge-triggering dissolves both: the schedule acts only when its answer FLIPS, so
// an override survives without anything gating the scheduler.
#include <unity.h>

#include "../../src/utils/RestPolicy.h"
#include "../../src/utils/RestPolicy.cpp"

// ------------------------------------------------------------ resolveSchedule

// THE most important behaviour in the file. The schedule is asked again, says the
// same thing, and must therefore say NOTHING -- leaving a restActive that
// disagrees with it completely alone. Delete this property and bug 2 returns.
void test_no_flip_leaves_a_manual_override_untouched(void)
{
    // Scheduled rest is in force, but a human woke the panel.
    const RestDecision d = resolveSchedule(/*restActive*/ false, /*manual*/ true,
                                           SCHEDULE_REST, /*restNow*/ true);
    TEST_ASSERT_FALSE(d.restActive);            // still awake
    TEST_ASSERT_TRUE(d.manual);                 // still flagged as an override
    TEST_ASSERT_EQUAL_INT(SCHEDULE_REST, d.opinion);
    TEST_ASSERT_FALSE(d.changed);               // nothing to signal
}

// At boot there is no previous opinion, so the schedule must assert itself once.
void test_the_first_evaluation_always_counts_as_a_flip(void)
{
    const RestDecision d = resolveSchedule(false, false, SCHEDULE_UNKNOWN, true);
    TEST_ASSERT_TRUE(d.restActive);
    TEST_ASSERT_EQUAL_INT(SCHEDULE_REST, d.opinion);
    TEST_ASSERT_TRUE(d.changed);
}

void test_a_flip_into_a_rest_window_turns_the_display_off(void)
{
    const RestDecision d = resolveSchedule(false, false, SCHEDULE_AWAKE, true);
    TEST_ASSERT_TRUE(d.restActive);
    TEST_ASSERT_EQUAL_INT(SCHEDULE_REST, d.opinion);
    TEST_ASSERT_TRUE(d.changed);
}

void test_a_flip_out_of_a_rest_window_wakes_the_display(void)
{
    const RestDecision d = resolveSchedule(true, false, SCHEDULE_REST, false);
    TEST_ASSERT_FALSE(d.restActive);
    TEST_ASSERT_EQUAL_INT(SCHEDULE_AWAKE, d.opinion);
    TEST_ASSERT_TRUE(d.changed);
}

// A flip is the schedule changing its mind, and that ends any disagreement --
// the override was "until the schedule next decides something new".
void test_a_flip_ends_a_manual_override(void)
{
    const RestDecision d = resolveSchedule(false, true, SCHEDULE_REST, false);
    TEST_ASSERT_FALSE(d.manual);
    TEST_ASSERT_EQUAL_INT(SCHEDULE_AWAKE, d.opinion);
}

// The schedule flipped, but the panel was already in the state it now wants
// (a human got there first). The opinion and the label must still update, yet
// there is nothing to redraw -- callers signal only on `changed`.
void test_a_flip_onto_the_current_state_updates_state_but_reports_no_change(void)
{
    const RestDecision d = resolveSchedule(/*restActive*/ false, /*manual*/ true,
                                           SCHEDULE_REST, /*restNow*/ false);
    TEST_ASSERT_FALSE(d.restActive);
    TEST_ASSERT_FALSE(d.manual);
    TEST_ASSERT_EQUAL_INT(SCHEDULE_AWAKE, d.opinion);
    TEST_ASSERT_FALSE(d.changed);
}

// Regression guard for bug 2. Nothing gates the scheduler any more, so no number
// of presses can leave it disarmed: the very next flip still acts.
void test_the_schedule_still_acts_after_many_manual_presses(void)
{
    bool restActive = false;
    bool manual = false;
    int8_t opinion = SCHEDULE_AWAKE;

    for (int i = 0; i < 20; i++)
    {
        const RestDecision p = resolveManual(restActive, (i % 2) == 0, opinion);
        restActive = p.restActive;
        manual = p.manual;
        opinion = p.opinion; // a press must never move this
    }
    TEST_ASSERT_EQUAL_INT(SCHEDULE_AWAKE, opinion);

    const RestDecision d = resolveSchedule(restActive, manual, opinion, /*restNow*/ true);
    TEST_ASSERT_TRUE(d.restActive);
    TEST_ASSERT_FALSE(d.manual);
    TEST_ASSERT_EQUAL_INT(SCHEDULE_REST, d.opinion);
}

// -------------------------------------------------------------- resolveManual

// Regression guard for bug 1: the panel is asleep because the SCHEDULE said so,
// and the button must still wake it.
void test_pressing_wake_during_a_scheduled_rest_wakes_the_panel(void)
{
    const RestDecision d = resolveManual(/*restActive*/ true, /*enabled*/ false, SCHEDULE_REST);
    TEST_ASSERT_FALSE(d.restActive);
    TEST_ASSERT_TRUE(d.manual);   // now disagreeing with the schedule
    TEST_ASSERT_TRUE(d.changed);  // caller must restore brightness and resume fetching
}

// A press is a disagreement, not a lesson. If it moved the opinion, the next
// evaluation would see a false flip and the override would evaporate.
void test_a_press_never_changes_the_schedules_opinion(void)
{
    TEST_ASSERT_EQUAL_INT(SCHEDULE_REST, resolveManual(true, false, SCHEDULE_REST).opinion);
    TEST_ASSERT_EQUAL_INT(SCHEDULE_AWAKE, resolveManual(false, true, SCHEDULE_AWAKE).opinion);
    TEST_ASSERT_EQUAL_INT(SCHEDULE_UNKNOWN, resolveManual(false, true, SCHEDULE_UNKNOWN).opinion);
}

// `manual` is DERIVED, so the status banner cannot claim an override that is not
// one: pressing the panel into the state the schedule already wants agrees with it.
void test_pressing_into_the_state_the_schedule_wants_is_not_an_override(void)
{
    TEST_ASSERT_FALSE(resolveManual(false, true, SCHEDULE_REST).manual);
    TEST_ASSERT_FALSE(resolveManual(true, false, SCHEDULE_AWAKE).manual);
}

// With no opinion yet we cannot claim agreement, so a press counts as an override.
void test_a_press_before_the_schedule_has_an_opinion_is_an_override(void)
{
    TEST_ASSERT_TRUE(resolveManual(false, true, SCHEDULE_UNKNOWN).manual);
    TEST_ASSERT_TRUE(resolveManual(true, false, SCHEDULE_UNKNOWN).manual);
}

// Pressing for the state the panel is already in changes nothing to redraw, and
// must not trigger the wake side effects (a departures refetch, a ticker refetch).
void test_a_press_for_the_current_state_reports_no_change(void)
{
    TEST_ASSERT_FALSE(resolveManual(true, true, SCHEDULE_REST).changed);
    TEST_ASSERT_FALSE(resolveManual(false, false, SCHEDULE_AWAKE).changed);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_no_flip_leaves_a_manual_override_untouched);
    RUN_TEST(test_the_first_evaluation_always_counts_as_a_flip);
    RUN_TEST(test_a_flip_into_a_rest_window_turns_the_display_off);
    RUN_TEST(test_a_flip_out_of_a_rest_window_wakes_the_display);
    RUN_TEST(test_a_flip_ends_a_manual_override);
    RUN_TEST(test_a_flip_onto_the_current_state_updates_state_but_reports_no_change);
    RUN_TEST(test_the_schedule_still_acts_after_many_manual_presses);
    RUN_TEST(test_pressing_wake_during_a_scheduled_rest_wakes_the_panel);
    RUN_TEST(test_a_press_never_changes_the_schedules_opinion);
    RUN_TEST(test_pressing_into_the_state_the_schedule_wants_is_not_an_override);
    RUN_TEST(test_a_press_before_the_schedule_has_an_opinion_is_an_override);
    RUN_TEST(test_a_press_for_the_current_state_reports_no_change);
    return UNITY_END();
}
