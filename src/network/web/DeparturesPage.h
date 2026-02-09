#ifndef DEPARTURES_PAGE_H
#define DEPARTURES_PAGE_H

#include <Arduino.h>
#include "../../api/DepartureData.h"

// Build the departures viewer HTML page
// @param deps Array of cached departures (local copy, safe to read)
// @param count Number of departures in array
// @return Complete HTML page string
String buildDeparturesPage(const Departure* deps, int count);

// Build JSON response with current departure data for AJAX refresh
// @param deps Array of cached departures (local copy, safe to read)
// @param count Number of departures in array
// @return JSON string
String buildDeparturesJson(const Departure* deps, int count);

#endif // DEPARTURES_PAGE_H
