#ifndef TRANSIT_DATA_TAB_H
#define TRANSIT_DATA_TAB_H

#include "../../../config/AppConfig.h"
#include <Arduino.h>

/**
 * Build the Transit Data tab content
 * Contains API configuration for Prague, Berlin, and MQTT (dynamic based on city)
 */
String buildTransitDataTab(const Config* config);

#endif // TRANSIT_DATA_TAB_H
