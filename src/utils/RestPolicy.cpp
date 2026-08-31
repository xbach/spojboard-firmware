#include "RestPolicy.h"

RestDecision resolveSchedule(bool restActive, bool manual, int8_t lastOpinion, bool restNow)
{
    RestDecision d = {restActive, manual, lastOpinion, false};

    const int8_t nowOpinion = restNow ? SCHEDULE_REST : SCHEDULE_AWAKE;

    // No flip -> the schedule has nothing new to say, so it says nothing. Any
    // manual override survives untouched. Removing this branch restores
    // level-triggering, and with it the dead-button and permanently-disarmed
    // pair of bugs described in the header.
    if (lastOpinion != SCHEDULE_UNKNOWN && nowOpinion == lastOpinion)
    {
        return d;
    }

    d.opinion = nowOpinion;
    d.restActive = restNow;
    d.manual = false; // a flip ends any override
    d.changed = (d.restActive != restActive);
    return d;
}

RestDecision resolveManual(bool restActive, bool enabled, int8_t lastOpinion)
{
    RestDecision d;
    d.restActive = enabled;
    d.opinion = lastOpinion; // a press never teaches the scheduler anything
    // Derived, not hardcoded: pressing the panel into the state the schedule
    // already wants is not an override, and the banner must not claim it is.
    // With no opinion yet we cannot claim agreement, so it counts as one.
    d.manual = (lastOpinion == SCHEDULE_UNKNOWN) || (enabled != (lastOpinion == SCHEDULE_REST));
    d.changed = (enabled != restActive);
    return d;
}
