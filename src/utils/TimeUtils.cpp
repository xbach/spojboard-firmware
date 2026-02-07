#include "TimeUtils.h"
#include "Logger.h"
#include <Arduino.h>
#include <cstring>

void initTimeSync()
{
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
}

bool syncTime(int maxAttempts, int delayMs)
{
    logTimestamp();
    Serial.println("Syncing time via NTP...");

    struct tm timeinfo;
    int attempts = 0;

    while (!getLocalTime(&timeinfo) && attempts < maxAttempts)
    {
        delay(delayMs);
        attempts++;

        // Log progress for long syncs
        if (attempts % 3 == 0)
        {
            logTimestamp();
            Serial.print("NTP sync attempt ");
            Serial.print(attempts);
            Serial.print("/");
            Serial.println(maxAttempts);
        }
    }

    if (attempts >= maxAttempts)
    {
        logTimestamp();
        Serial.println("⚠️ NTP SYNC FAILED - Device clock may be incorrect!");
        return false;
    }

    // Get current time as unix timestamp for verification
    time_t now;
    time(&now);

    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    logTimestamp();
    Serial.print("✓ NTP sync successful: ");
    Serial.print(timeStr);
    Serial.print(" (unix=");
    Serial.print((int)now);
    Serial.println(")");

    return true;
}

bool getFormattedTime(char* buffer, size_t size, const char* format)
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        return false;
    }

    strftime(buffer, size, format, &timeinfo);
    return true;
}

bool getCurrentTime(struct tm* timeinfo)
{
    return getLocalTime(timeinfo);
}

bool isTimeSynced()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        return false;
    }

    // Check if year is reasonable (after 2020)
    // ESP32 defaults to epoch (1970) or compilation date if NTP fails
    return (timeinfo.tm_year + 1900) >= 2020;
}

time_t parseTimestamp(const char* timestamp, const char* format)
{
    if (!timestamp)
    {
        return -1;
    }

    struct tm tm;
    memset(&tm, 0, sizeof(tm)); // Initialize all fields to zero

    // Parse the timestamp string
    if (strptime(timestamp, format, &tm) == NULL)
    {
        return -1; // Parse failed
    }

    // Let mktime() auto-determine DST based on configured timezone
    // This is critical: -1 means "use timezone rules to decide"
    // 0 would mean "standard time only", 1 would mean "DST only"
    tm.tm_isdst = -1;

    return mktime(&tm);
}

// ============================================================================
// Localized Date/Time Strings (PROGMEM)
// ============================================================================

// English full day names
static const char DAY_FULL_EN_0[] PROGMEM = "Sunday";
static const char DAY_FULL_EN_1[] PROGMEM = "Monday";
static const char DAY_FULL_EN_2[] PROGMEM = "Tuesday";
static const char DAY_FULL_EN_3[] PROGMEM = "Wednesday";
static const char DAY_FULL_EN_4[] PROGMEM = "Thursday";
static const char DAY_FULL_EN_5[] PROGMEM = "Friday";
static const char DAY_FULL_EN_6[] PROGMEM = "Saturday";
static const char* const DAYS_FULL_EN[] PROGMEM = {DAY_FULL_EN_0, DAY_FULL_EN_1, DAY_FULL_EN_2, DAY_FULL_EN_3, DAY_FULL_EN_4, DAY_FULL_EN_5, DAY_FULL_EN_6};

// Czech full day names
static const char DAY_FULL_CS_0[] PROGMEM = "Neděle";
static const char DAY_FULL_CS_1[] PROGMEM = "Pondělí";
static const char DAY_FULL_CS_2[] PROGMEM = "Úterý";
static const char DAY_FULL_CS_3[] PROGMEM = "Středa";
static const char DAY_FULL_CS_4[] PROGMEM = "Čtvrtek";
static const char DAY_FULL_CS_5[] PROGMEM = "Pátek";
static const char DAY_FULL_CS_6[] PROGMEM = "Sobota";
static const char* const DAYS_FULL_CS[] PROGMEM = {DAY_FULL_CS_0, DAY_FULL_CS_1, DAY_FULL_CS_2, DAY_FULL_CS_3, DAY_FULL_CS_4, DAY_FULL_CS_5, DAY_FULL_CS_6};

// German full day names
static const char DAY_FULL_DE_0[] PROGMEM = "Sonntag";
static const char DAY_FULL_DE_1[] PROGMEM = "Montag";
static const char DAY_FULL_DE_2[] PROGMEM = "Dienstag";
static const char DAY_FULL_DE_3[] PROGMEM = "Mittwoch";
static const char DAY_FULL_DE_4[] PROGMEM = "Donnerstag";
static const char DAY_FULL_DE_5[] PROGMEM = "Freitag";
static const char DAY_FULL_DE_6[] PROGMEM = "Samstag";
static const char* const DAYS_FULL_DE[] PROGMEM = {DAY_FULL_DE_0, DAY_FULL_DE_1, DAY_FULL_DE_2, DAY_FULL_DE_3, DAY_FULL_DE_4, DAY_FULL_DE_5, DAY_FULL_DE_6};

const char* getLocalizedDayFull(int tm_wday, const char* lang)
{
    if (tm_wday < 0 || tm_wday > 6)
        tm_wday = 0;

    if (strcmp(lang, "cs") == 0)
    {
        return (const char*)pgm_read_ptr(&DAYS_FULL_CS[tm_wday]);
    }
    else if (strcmp(lang, "de") == 0)
    {
        return (const char*)pgm_read_ptr(&DAYS_FULL_DE[tm_wday]);
    }
    return (const char*)pgm_read_ptr(&DAYS_FULL_EN[tm_wday]);
}
