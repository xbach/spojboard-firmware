#ifndef TIMEUTILS_H
#define TIMEUTILS_H

#include <time.h>

// ============================================================================
// Time Configuration
// ============================================================================

// NTP Server
#define NTP_SERVER "pool.ntp.org"

// Timezone: CET/CEST (Prague)
#define GMT_OFFSET_SEC 3600 // CET = UTC+1
#define DAYLIGHT_OFFSET_SEC 3600 // CEST = UTC+2

// ============================================================================
// Time Synchronization Functions
// ============================================================================

/**
 * Initialize NTP time synchronization
 * Configures NTP client with Prague timezone (CET/CEST)
 */
void initTimeSync();

/**
 * Wait for NTP time synchronization to complete
 * @param maxAttempts Maximum number of sync attempts (default: 10)
 * @param delayMs Delay between attempts in milliseconds (default: 500)
 * @return true if sync succeeded, false if timeout
 */
bool syncTime(int maxAttempts = 10, int delayMs = 500);

/**
 * Get formatted time string
 * @param buffer Buffer to write formatted time string
 * @param size Size of buffer
 * @param format strftime format string (default: "%Y-%m-%d %H:%M:%S")
 * @return true if successful, false if time not set
 */
bool getFormattedTime(char* buffer, size_t size, const char* format = "%Y-%m-%d %H:%M:%S");

/**
 * Get current local time
 * @param timeinfo Pointer to tm struct to fill
 * @return true if successful, false if time not set
 */
bool getCurrentTime(struct tm* timeinfo);

/**
 * Check if device time is reasonable (after year 2020)
 * Used to verify NTP sync actually worked
 * @return true if time appears to be synced, false if still at epoch/default
 */
bool isTimeSynced();

/**
 * Parse ISO 8601 timestamp to Unix time
 * Properly initializes tm struct and lets mktime() auto-determine DST
 * @param timestamp ISO 8601 timestamp string (e.g., "2026-02-07T21:43:48+01:00")
 * @param format strptime format string (default: ISO 8601 basic format)
 * @return Unix timestamp (time_t), or -1 on parse error
 */
time_t parseTimestamp(const char* timestamp, const char* format = "%Y-%m-%dT%H:%M:%S");

// ============================================================================
// Localized Date/Time Functions
// ============================================================================

/**
 * Get localized day of week abbreviation (3 characters)
 * @param tm_wday Day of week (0=Sunday, 6=Saturday)
 * @param lang Language code: "en", "cs", or "de"
 * @return Pointer to day abbreviation string (PROGMEM)
 */
const char* getLocalizedDay(int tm_wday, const char* lang);

/**
 * Get localized month abbreviation (3 characters)
 * @param tm_mon Month (0=January, 11=December)
 * @param lang Language code: "en", "cs", or "de"
 * @return Pointer to month abbreviation string (PROGMEM)
 */
const char* getLocalizedMonth(int tm_mon, const char* lang);

#endif // TIMEUTILS_H
