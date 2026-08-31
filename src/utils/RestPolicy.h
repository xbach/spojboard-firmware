#ifndef REST_POLICY_H
#define REST_POLICY_H

// ============================================================================
// Rest policy -- who decides whether the panel is lit (TA-0254)
// ============================================================================
// Two inputs can turn the panel off: the rest SCHEDULE (config.restModePeriods)
// and a human pressing the header button. This file owns the arbitration, and
// nothing else may.
//
// The schedule is EDGE-TRIGGERED: it acts only when its answer FLIPS, never
// merely because it was asked again. That is the whole design.
//
// The previous scheduler was level-triggered -- it re-asserted its answer at
// every :00/:30 -- so the only way to let a manual press stick was to switch the
// scheduler off, via `if (!restModeManual)` in checkScheduledRestMode(). That
// bought a working override at the price of two bugs:
//
//   1. One press disarmed the schedule until the next press.
//   2. The button was DEAD during a scheduled rest, because it rendered from
//      (restModeActive && restModeManual) -- false during a scheduled rest -- and
//      onRestMode()'s guards then matched neither branch. A silent no-op.
//
// With edge-triggering a manual choice is honoured simply because the schedule
// stays quiet until it changes its mind. Nothing gates the scheduler, so nothing
// can leave it disarmed.
//
// `manual` is therefore OUTPUT ONLY -- a label meaning "the panel currently
// disagrees with the schedule". NOTHING may read it to make a decision; doing so
// reintroduces bug 1.
//
// NOTE THE POLARITY. SpojBoard configures when the display is OFF (rest windows,
// isInRestPeriod()); the BeerBoard original this pattern came from configures
// when the bar is OPEN. The vocabulary here is SpojBoard's -- `restNow` is "the
// schedule says rest right now" -- so do not port reasoning between them without
// re-deriving it. An empty restModePeriods makes isInRestPeriod() false, i.e.
// "never rest", which is the safe direction for a departure board.
//
// Pure: no Arduino, no time, no display. Tested natively in test/test_restmode.

#include <stdint.h>

// A schedule opinion. UNKNOWN means "never evaluated" -- true at boot, and it
// guarantees the first evaluation counts as a flip, so a fresh boot always hands
// control to the schedule.
#define SCHEDULE_UNKNOWN (-1)
#define SCHEDULE_AWAKE 0
#define SCHEDULE_REST 1

struct RestDecision
{
    bool restActive; // resulting display state (true = display OFF)
    bool manual;     // resulting restModeManual (UI label only, see above)
    int8_t opinion;  // resulting lastScheduleOpinion
    bool changed;    // did restActive change? callers act ONLY when true
};

// Applies a fresh schedule evaluation.
//
// On a NO-FLIP (restNow agrees with lastOpinion) every field is returned
// unchanged -- including a restActive that disagrees with the schedule. That is
// the manual override holding, and it is the single most important behaviour
// here.
//
// On a FLIP the schedule takes over: restActive follows restNow and manual is
// cleared. lastOpinion == SCHEDULE_UNKNOWN always counts as a flip.
RestDecision resolveSchedule(bool restActive, bool manual, int8_t lastOpinion, bool restNow);

// Applies a manual press. `enabled` follows the POST /rest-mode wire format:
// true means REST, i.e. display OFF.
//
// Never changes the schedule's opinion -- a press does not teach the scheduler
// anything, it just disagrees with it until the next flip. If it did move the
// opinion, the next evaluation would see a false flip and the override would
// evaporate.
//
// `manual` is DERIVED, not hardcoded: pressing the panel into the state the
// schedule already wants reports no override, so the banner cannot lie.
RestDecision resolveManual(bool restActive, bool enabled, int8_t lastOpinion);

#endif // REST_POLICY_H
