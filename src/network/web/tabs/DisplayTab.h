#ifndef DISPLAY_TAB_H
#define DISPLAY_TAB_H

#include "../../../config/AppConfig.h"
#include <Arduino.h>

/**
 * Build the Display tab content
 * Contains brightness, departures, locale, line colors, display options
 */
String buildDisplayTab(const Config* config);

#endif // DISPLAY_TAB_H
