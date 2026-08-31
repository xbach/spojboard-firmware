#ifndef CONNECTION_TAB_H
#define CONNECTION_TAB_H

#include <Arduino.h> // String -- AppConfig.h no longer pulls it in transitively
#include "../../../config/AppConfig.h"

/**
 * Build Connection tab content
 *
 * Contains WiFi credentials and data source selector
 * Visible in both AP and STA modes
 */
String buildConnectionTab(const Config* config, bool apModeActive);

#endif
