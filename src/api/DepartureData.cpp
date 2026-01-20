#include "DepartureData.h"
#include <string.h>

// ============================================================================
// String Shortening for Display
// ============================================================================

struct StringReplacement
{
    const char* search;
    const char* replace;
};

// Common Czech words to shorten for display space
// Note: Strings are in UTF-8 format (before conversion to ISO-8859-2)
static const StringReplacement replacements[] = {
    {"Nádraží", "Nádr."},
    {"Sídliště", "Sídl."},
    {"Nemocnice", "Nem."},
    {"nádraží", "nádr."},
    {"S+U Hauptbahnhof", "Hauptbahnhof"},
    {" Hauptbahnhof", " Hbf"},
    {"Bahnhof", "Bhf"},
    {"(Berlin)", "(B)"},
    {" Richtung ", "->"},
};
static const int replacementCount = sizeof(replacements) / sizeof(replacements[0]);

void shortenDestination(char* destination)
{
    for (int i = 0; i < replacementCount; i++)
    {
        char* pos = strstr(destination, replacements[i].search);
        if (pos != NULL)
        {
            int searchLen = strlen(replacements[i].search);
            int replaceLen = strlen(replacements[i].replace);
            int tailLen = strlen(pos + searchLen);

            // Copy replacement
            memcpy(pos, replacements[i].replace, replaceLen);
            // Shift remaining text
            memmove(pos + replaceLen, pos + searchLen, tailLen + 1); // +1 for null terminator
        }
    }
}

// ============================================================================
// Departure Sorting Helper
// ============================================================================

int compareDepartures(const void* a, const void* b)
{
    Departure* depA = (Departure*)a;
    Departure* depB = (Departure*)b;
    return depA->eta - depB->eta; // Sort by ETA ascending
}

// ============================================================================
// ETA Calculation Helper
// ============================================================================

int calculateETA(time_t departureTime)
{
    time_t now;
    time(&now);
    int diffSec = difftime(departureTime, now);
    return (diffSec > 0) ? (diffSec / 60) : 0;
}

// ============================================================================
// String Space Removal
// ============================================================================

void stripSpaces(char* str)
{
    char* src = str;
    char* dst = str;
    while (*src)
    {
        if (*src != ' ')
        {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
}

// ============================================================================
// String Bracket Removal
// ============================================================================

void stripBrackets(char* str)
{
    char* src = str;
    char* dst = str;
    while (*src)
    {
        char c = *src;
        if (c != '<' && c != '>' && c != '[' && c != ']' && c != '{' && c != '}' && c != '(' && c != ')')
        {
            *dst++ = c;
        }
        src++;
    }
    *dst = '\0';
}
