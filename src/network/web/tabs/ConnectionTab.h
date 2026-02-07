#ifndef CONNECTION_TAB_H
#define CONNECTION_TAB_H

#include "../../../config/AppConfig.h"

/**
 * Build Connection tab content
 *
 * Contains WiFi credentials and data source selector
 * Visible in both AP and STA modes
 */
String buildConnectionTab(const Config* config, bool apModeActive);

#endif
