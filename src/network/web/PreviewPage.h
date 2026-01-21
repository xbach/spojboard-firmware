#ifndef PREVIEW_PAGE_H
#define PREVIEW_PAGE_H

#include <Arduino.h>

/**
 * Build the live display preview HTML page
 * Shows a real-time view of the LED matrix display with all state information
 * @return Complete HTML page string
 */
String buildPreviewPage();

#endif // PREVIEW_PAGE_H
