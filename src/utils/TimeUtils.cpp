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
    Serial.println("Syncing time...");

    struct tm timeinfo;
    int attempts = 0;

    while (!getLocalTime(&timeinfo) && attempts < maxAttempts)
    {
        delay(delayMs);
        attempts++;
    }

    if (attempts >= maxAttempts)
    {
        logTimestamp();
        Serial.println("Time sync failed!");
        return false;
    }

    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    logTimestamp();
    Serial.print("Time synced: ");
    Serial.println(timeStr);

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

// ============================================================================
// Localized Date/Time Strings (PROGMEM)
// ============================================================================

// English day abbreviations (3 chars)
static const char DAY_EN_0[] PROGMEM = "Sun";
static const char DAY_EN_1[] PROGMEM = "Mon";
static const char DAY_EN_2[] PROGMEM = "Tue";
static const char DAY_EN_3[] PROGMEM = "Wed";
static const char DAY_EN_4[] PROGMEM = "Thu";
static const char DAY_EN_5[] PROGMEM = "Fri";
static const char DAY_EN_6[] PROGMEM = "Sat";
static const char* const DAYS_EN[] PROGMEM = {DAY_EN_0, DAY_EN_1, DAY_EN_2, DAY_EN_3, DAY_EN_4, DAY_EN_5, DAY_EN_6};

// Czech day abbreviations (3 chars)
static const char DAY_CS_0[] PROGMEM = "Ned";
static const char DAY_CS_1[] PROGMEM = "Pon";
static const char DAY_CS_2[] PROGMEM = "Úte";
static const char DAY_CS_3[] PROGMEM = "Stř";
static const char DAY_CS_4[] PROGMEM = "Čtv";
static const char DAY_CS_5[] PROGMEM = "Pát";
static const char DAY_CS_6[] PROGMEM = "Sob";
static const char* const DAYS_CS[] PROGMEM = {DAY_CS_0, DAY_CS_1, DAY_CS_2, DAY_CS_3, DAY_CS_4, DAY_CS_5, DAY_CS_6};

// German day abbreviations (3 chars)
static const char DAY_DE_0[] PROGMEM = "Son";
static const char DAY_DE_1[] PROGMEM = "Mon";
static const char DAY_DE_2[] PROGMEM = "Die";
static const char DAY_DE_3[] PROGMEM = "Mit";
static const char DAY_DE_4[] PROGMEM = "Don";
static const char DAY_DE_5[] PROGMEM = "Fre";
static const char DAY_DE_6[] PROGMEM = "Sam";
static const char* const DAYS_DE[] PROGMEM = {DAY_DE_0, DAY_DE_1, DAY_DE_2, DAY_DE_3, DAY_DE_4, DAY_DE_5, DAY_DE_6};

// English month abbreviations (3 chars)
static const char MON_EN_0[] PROGMEM = "Jan";
static const char MON_EN_1[] PROGMEM = "Feb";
static const char MON_EN_2[] PROGMEM = "Mar";
static const char MON_EN_3[] PROGMEM = "Apr";
static const char MON_EN_4[] PROGMEM = "May";
static const char MON_EN_5[] PROGMEM = "Jun";
static const char MON_EN_6[] PROGMEM = "Jul";
static const char MON_EN_7[] PROGMEM = "Aug";
static const char MON_EN_8[] PROGMEM = "Sep";
static const char MON_EN_9[] PROGMEM = "Oct";
static const char MON_EN_10[] PROGMEM = "Nov";
static const char MON_EN_11[] PROGMEM = "Dec";
static const char* const MONTHS_EN[] PROGMEM = {MON_EN_0,
                                                MON_EN_1,
                                                MON_EN_2,
                                                MON_EN_3,
                                                MON_EN_4,
                                                MON_EN_5,
                                                MON_EN_6,
                                                MON_EN_7,
                                                MON_EN_8,
                                                MON_EN_9,
                                                MON_EN_10,
                                                MON_EN_11};

// Czech month abbreviations (3 chars)
static const char MON_CS_0[] PROGMEM = "Led";
static const char MON_CS_1[] PROGMEM = "Úno";
static const char MON_CS_2[] PROGMEM = "Bře";
static const char MON_CS_3[] PROGMEM = "Dub";
static const char MON_CS_4[] PROGMEM = "Kvě";
static const char MON_CS_5[] PROGMEM = "Čvn";
static const char MON_CS_6[] PROGMEM = "Čvc";
static const char MON_CS_7[] PROGMEM = "Srp";
static const char MON_CS_8[] PROGMEM = "Zář";
static const char MON_CS_9[] PROGMEM = "Říj";
static const char MON_CS_10[] PROGMEM = "Lis";
static const char MON_CS_11[] PROGMEM = "Pro";
static const char* const MONTHS_CS[] PROGMEM = {MON_CS_0,
                                                MON_CS_1,
                                                MON_CS_2,
                                                MON_CS_3,
                                                MON_CS_4,
                                                MON_CS_5,
                                                MON_CS_6,
                                                MON_CS_7,
                                                MON_CS_8,
                                                MON_CS_9,
                                                MON_CS_10,
                                                MON_CS_11};

// German month abbreviations (3 chars)
static const char MON_DE_0[] PROGMEM = "Jan";
static const char MON_DE_1[] PROGMEM = "Feb";
static const char MON_DE_2[] PROGMEM = "Mär";
static const char MON_DE_3[] PROGMEM = "Apr";
static const char MON_DE_4[] PROGMEM = "Mai";
static const char MON_DE_5[] PROGMEM = "Jun";
static const char MON_DE_6[] PROGMEM = "Jul";
static const char MON_DE_7[] PROGMEM = "Aug";
static const char MON_DE_8[] PROGMEM = "Sep";
static const char MON_DE_9[] PROGMEM = "Okt";
static const char MON_DE_10[] PROGMEM = "Nov";
static const char MON_DE_11[] PROGMEM = "Dez";
static const char* const MONTHS_DE[] PROGMEM = {MON_DE_0,
                                                MON_DE_1,
                                                MON_DE_2,
                                                MON_DE_3,
                                                MON_DE_4,
                                                MON_DE_5,
                                                MON_DE_6,
                                                MON_DE_7,
                                                MON_DE_8,
                                                MON_DE_9,
                                                MON_DE_10,
                                                MON_DE_11};

const char* getLocalizedDay(int tm_wday, const char* lang)
{
    if (tm_wday < 0 || tm_wday > 6)
        tm_wday = 0;

    if (strcmp(lang, "cs") == 0)
    {
        return (const char*)pgm_read_ptr(&DAYS_CS[tm_wday]);
    }
    else if (strcmp(lang, "de") == 0)
    {
        return (const char*)pgm_read_ptr(&DAYS_DE[tm_wday]);
    }
    return (const char*)pgm_read_ptr(&DAYS_EN[tm_wday]);
}

const char* getLocalizedMonth(int tm_mon, const char* lang)
{
    if (tm_mon < 0 || tm_mon > 11)
        tm_mon = 0;

    if (strcmp(lang, "cs") == 0)
    {
        return (const char*)pgm_read_ptr(&MONTHS_CS[tm_mon]);
    }
    else if (strcmp(lang, "de") == 0)
    {
        return (const char*)pgm_read_ptr(&MONTHS_DE[tm_mon]);
    }
    return (const char*)pgm_read_ptr(&MONTHS_EN[tm_mon]);
}
