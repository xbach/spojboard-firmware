#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

// ============================================================================
// Debug Logging Utilities
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
 * Initialize logger with config reference
 * Must be called before using conditional logging functions
 * @param cfg Pointer to config structure
 */
void initLogger(const struct Config* cfg);

/**
 * Print message to Serial and telnet (if debug enabled)
 * @param message Message to print
 */
void debugPrint(const char* message);

/**
 * Print message with newline to Serial and telnet (if debug enabled)
 * @param message Message to print
 */
void debugPrintln(const char* message);

/**
 * Print integer to Serial and telnet (if debug enabled)
 * @param value Integer value to print
 */
void debugPrint(int value);

/**
 * Print unsigned integer to Serial and telnet (if debug enabled)
 * Also handles size_t on ESP32 (where size_t == unsigned int)
 * @param value Unsigned integer value to print
 */
void debugPrint(unsigned int value);

#endif // LOGGER_H
