#ifndef DEPARTUREDATA_H
#define DEPARTUREDATA_H

#include <time.h>

// ============================================================================
// Departure Data Structures
// ============================================================================

#define DEPS_PER_STOP 10   // Departures requested per stop from API (BVG is ~2.2KB/dep; 10 keeps the response within the read buffer and largest free block on 4-panel builds)
#define MAX_DEPARTURES 24  // Result cache size (larger for secondEta matching across multi-stop hubs)

struct Departure
{
    char line[8]; // Line number (e.g., "31", "A", "S9")
    char destination[64]; // Destination/headsign
    int eta; // Minutes until departure (recalculated from departureTime)
    time_t departureTime; // Unix timestamp of departure (from API)
    char platform[8]; // Platform/track (e.g., "D", "3", optional)
    char sourceStopId[16]; // Stop ID that produced this departure (for symbol matching)
    bool hasAC; // Air conditioning
    bool isDelayed; // Has delay
    int delayMinutes; // Delay in minutes
    int secondEta; // ETA of next departure for same line+destination (-1 if none)
    time_t secondDepartureTime; // Timestamp of second departure (0 if none)
};

// ============================================================================
// Departure Processing Functions
// ============================================================================

/**
 * Comparison function for qsort - sorts departures by ETA ascending, then destination alphabetical
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

// ============================================================================
// Stop-ID list parsing (shared by Golemio and BVG multi-stop clients)
// ============================================================================

/**
 * Count the non-empty, comma-separated stop IDs in a list.
 * @param csv Comma-separated stop-ID string (e.g. "U693Z2P, U321Z1P")
 * @return Number of non-empty tokens
 */
int countStopIds(const char* csv);

/**
 * Extract the index-th non-empty, whitespace-trimmed stop ID from a list.
 * @param csv Comma-separated stop-ID string
 * @param index Zero-based index of the stop to extract
 * @param out Output buffer for the stop ID
 * @param outSize Size of the output buffer
 * @return true if a stop ID was written, false if index is out of range
 */
bool getStopIdAt(const char* csv, int index, char* out, size_t outSize);

#endif // DEPARTUREDATA_H
