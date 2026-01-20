#ifndef DEPARTUREDATA_H
#define DEPARTUREDATA_H

#include <time.h>

// ============================================================================
// Departure Data Structures
// ============================================================================

#define MAX_DEPARTURES 12 // Increased for better caching with filtering

struct Departure
{
    char line[8]; // Line number (e.g., "31", "A", "S9")
    char destination[64]; // Destination/headsign
    int eta; // Minutes until departure (recalculated from departureTime)
    time_t departureTime; // Unix timestamp of departure (from API)
    char platform[8]; // Platform/track (e.g., "D", "3", optional)
    bool hasAC; // Air conditioning
    bool isDelayed; // Has delay
    int delayMinutes; // Delay in minutes
};

// ============================================================================
// Departure Processing Functions
// ============================================================================

/**
 * Comparison function for qsort - sorts departures by ETA ascending
 * @param a First departure
 * @param b Second departure
 * @return Negative if a < b, positive if a > b, zero if equal
 */
int compareDepartures(const void* a, const void* b);

/**
 * Shorten long destination names to fit on display
 * Modifies the string in-place
 * @param destination UTF-8 string to shorten (will be modified in-place)
 */
void shortenDestination(char* destination);

/**
 * Calculate ETA in minutes from departure timestamp
 * @param departureTime Unix timestamp of departure
 * @return Minutes until departure (0 if already departed)
 */
int calculateETA(time_t departureTime);

/**
 * Remove all spaces from a string (leading, trailing, and middle)
 * Modifies the string in-place
 * @param str String to strip spaces from
 */
void stripSpaces(char* str);

/**
 * Remove all bracket characters from a string: < [ { ( ) } ] >
 * Modifies the string in-place
 * @param str String to strip brackets from
 */
void stripBrackets(char* str);

#endif // DEPARTUREDATA_H
