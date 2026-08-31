#ifndef HARDWARE_TAB_H
#define HARDWARE_TAB_H

#include "../../../config/AppConfig.h"
#include <Arduino.h>

/**
 * Build the Hardware tab content (TA-0302).
 *
 * Panel wiring: RGB channel order, driver chip, and the optional custom pin map.
 * Rendered in AP mode as well as STA -- a panel blanked by a bad pin map is a
 * plausible reason the user is standing in AP mode, so this must be reachable
 * from there.
 */
String buildHardwareTab(const Config* config);

#endif // HARDWARE_TAB_H
