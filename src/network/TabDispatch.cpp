#include "TabDispatch.h"

#include <string.h>

bool tabSubmitted(const char* postedTab, const char* tabName, bool apModeActive)
{
    // Total by construction: this decides whether config fields get WRITTEN, so
    // it must be defined for every input, not only the ones today's caller sends.
    if (postedTab == nullptr || tabName == nullptr)
        return false;

    // An explicit name is the strongest evidence there is that the tab was sent.
    // It is never overridden by a guess about what a given mode renders.
    if (strcmp(postedTab, tabName) == 0)
        return true;

    if (strcmp(postedTab, "all") != 0)
        return false;

    // "all" outside AP mode is honest -- every tab is on the page.
    if (!apModeActive)
        return true;

    // In AP mode it is not: only these two exist to have been submitted.
    return (strcmp(tabName, "connection") == 0) || (strcmp(tabName, "hardware") == 0);
}
