#ifndef DISPLAYCOLORS_H
#define DISPLAYCOLORS_H

#include <stdint.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// ============================================================================
// Color Definitions (RGB565 format)
// ============================================================================

extern uint16_t COLOR_WHITE;
extern uint16_t COLOR_YELLOW;
extern uint16_t COLOR_RED;
extern uint16_t COLOR_GREEN;
extern uint16_t COLOR_BLUE;
extern uint16_t COLOR_ORANGE;
extern uint16_t COLOR_PURPLE;
extern uint16_t COLOR_BLACK;
extern uint16_t COLOR_CYAN;

// ============================================================================
// Color Management Functions
// ============================================================================

/**
 * Initialize color constants from display panel
 * Must be called after display is initialized
 * @param display Pointer to initialized HUB75 display
 */
void initColors(MatrixPanel_I2S_DMA* display);

/**
 * Parse color name to RGB565 value
 * @param colorName Color name (RED, GREEN, BLUE, YELLOW, ORANGE, PURPLE, CYAN, WHITE)
 * @return RGB565 color value, or 0 if invalid
 */
uint16_t parseColorName(const char* colorName);

/**
 * Get line color from configuration map
 * Three-pass matching: exact match -> fixed-length patterns (* only) -> flexible patterns (with ?)
 *
 * Wildcards (* = required position, ? = optional position after *):
 * - * matches any single char (* alone = any 1-char line)
 * - 9* matches exactly 2-char lines starting with 9
 * - S*? matches 2 or 3-char lines starting with S
 * - *??? matches any line of 1-4 chars (practical catch-all)
 *
 * Rules: ? must follow * or ?, no chars after ?, no wildcards in prefix
 *
 * @param line Line number/code
 * @param configMap Configuration string (format: "A=GREEN,B=YELLOW,9*=CYAN,S*?=BLUE,...")
 * @return RGB565 color value (COLOR_WHITE if nothing matches)
 */
uint16_t getLineColorWithConfig(const char* line, const char* configMap);

#endif // DISPLAYCOLORS_H
