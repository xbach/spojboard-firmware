#ifndef RESTMODE_H
#define RESTMODE_H

#include <time.h>

/**
 * Check if current time falls within any configured rest period
 * @param restPeriods Comma-separated time ranges (format: "HH:MM-HH:MM,HH:MM-HH:MM")
 * @return true if current time is within a rest period, false otherwise
 */
bool isInRestPeriod(const char* restPeriods);

/**
 * Parse single time string to hours and minutes
 * @param timeStr Time string in "HH:MM" format
 * @param hours Output: hours (0-23)
 * @param minutes Output: minutes (0-59)
 * @return true if parsing succeeded, false if invalid format
 */
bool parseTime(const char* timeStr, int& hours, int& minutes);

/**
 * Check if current time is between two time points (handles cross-midnight)
 * @param nowHour Current hour (0-23)
 * @param nowMin Current minute (0-59)
 * @param startHour Start hour (0-23)
 * @param startMin Start minute (0-59)
 * @param endHour End hour (0-23)
 * @param endMin End minute (0-59)
 * @return true if current time is within range
 */
bool isTimeBetween(int nowHour, int nowMin, int startHour, int startMin, int endHour, int endMin);

#endif // RESTMODE_H
