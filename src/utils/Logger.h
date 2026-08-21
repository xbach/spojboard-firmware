#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

// ============================================================================
// Debug Logging Utilities
//
// Everything here goes to Serial and nowhere else. Telnet mirroring was removed
// (it cost a dependency and a listening port for a channel nobody used); the
// separate `config.debugMode` flag survives and still gates the verbose HTTP /
// API chunk logging in HttpUtils and the transit clients.
// ============================================================================

/**
 * Print timestamp in format [milliseconds]
 */
void logTimestamp();

/**
 * Log memory usage with location label
 * @param location Label for this memory checkpoint
 */
void logMemory(const char* location);

/**
 * Print message to Serial
 * @param message Message to print
 */
void debugPrint(const char* message);

/**
 * Print message with newline to Serial
 * @param message Message to print
 */
void debugPrintln(const char* message);

/**
 * Print integer to Serial
 * @param value Integer value to print
 */
void debugPrint(int value);

/**
 * Print unsigned integer to Serial
 * Also handles size_t on ESP32 (where size_t == unsigned int)
 * @param value Unsigned integer value to print
 */
void debugPrint(unsigned int value);

#endif // LOGGER_H
