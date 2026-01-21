#ifndef WEB_UTILS_H
#define WEB_UTILS_H

#include <Arduino.h>

/**
 * Count the number of stops in a comma-separated list
 * @param stopIds Comma-separated stop IDs string
 * @return Number of stops (0 if null/empty)
 */
int countStops(const char* stopIds);

/**
 * Escape special characters in a string for JSON output
 * @param str String to escape
 * @return Escaped string safe for JSON
 */
String escapeJsonString(const char* str);

#endif // WEB_UTILS_H
