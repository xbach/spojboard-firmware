#include "RestMode.h"
#include "TimeUtils.h"
#include "Logger.h"
#include <string.h>
#include <stdlib.h>

bool parseTime(const char* timeStr, int& hours, int& minutes)
{
    if (!timeStr || strlen(timeStr) < 5)
    {
        return false;
    }

    // Expected format: "HH:MM"
    if (timeStr[2] != ':')
    {
        return false;
    }

    char hourStr[3] = {timeStr[0], timeStr[1], '\0'};
    char minStr[3] = {timeStr[3], timeStr[4], '\0'};

    hours = atoi(hourStr);
    minutes = atoi(minStr);

    // Validate ranges
    if (hours < 0 || hours > 23 || minutes < 0 || minutes > 59)
    {
        return false;
    }

    return true;
}

bool isTimeBetween(int nowHour, int nowMin, int startHour, int startMin, int endHour, int endMin)
{
    // Convert to minutes since midnight for easier comparison
    int nowMinutes = nowHour * 60 + nowMin;
    int startMinutes = startHour * 60 + startMin;
    int endMinutes = endHour * 60 + endMin;

    if (startMinutes <= endMinutes)
    {
        // Normal range (no midnight crossing): e.g., 09:00-17:00
        return (nowMinutes >= startMinutes && nowMinutes < endMinutes);
    }
    else
    {
        // Cross-midnight range: e.g., 22:00-06:00
        // True if after start OR before end
        return (nowMinutes >= startMinutes || nowMinutes < endMinutes);
    }
}

bool isInRestPeriod(const char* restPeriods)
{
    // Empty config = rest mode disabled
    if (!restPeriods || strlen(restPeriods) == 0)
    {
        return false;
    }

    // Get current time
    struct tm timeinfo;
    if (!getCurrentTime(&timeinfo))
    {
        // Time not available, assume not in rest period
        return false;
    }

    int nowHour = timeinfo.tm_hour;
    int nowMin = timeinfo.tm_min;

    // Parse comma-separated periods
    char periodsCopy[256];
    strlcpy(periodsCopy, restPeriods, sizeof(periodsCopy));

    char* token = strtok(periodsCopy, ",");
    while (token != nullptr)
    {
        // Trim whitespace
        while (*token == ' ')
            token++;

        // Expected format: "HH:MM-HH:MM"
        char* dashPos = strchr(token, '-');
        if (dashPos != nullptr)
        {
            *dashPos = '\0';
            const char* startStr = token;
            const char* endStr = dashPos + 1;

            int startHour, startMin, endHour, endMin;
            if (parseTime(startStr, startHour, startMin) && parseTime(endStr, endHour, endMin))
            {

                if (isTimeBetween(nowHour, nowMin, startHour, startMin, endHour, endMin))
                {
                    // Current time is within this rest period
                    return true;
                }
            }
            else
            {
                // Invalid format - log warning and skip
                logTimestamp();
                debugPrint("RestMode: Invalid time format: ");
                debugPrintln(token);
            }
        }

        token = strtok(nullptr, ",");
    }

    return false;
}
