#ifndef UPDATE_PAGE_H
#define UPDATE_PAGE_H

#include <Arduino.h>

// Build the OTA update HTML page
// Returns a String containing the complete HTML
String buildUpdatePage();

// Build the OTA blocked (AP mode) HTML page
String buildUpdateBlockedPage();

// Build the OTA success HTML page
String buildUpdateSuccessPage();

// Build the OTA error HTML page
String buildUpdateErrorPage(const char* errorMsg);

#endif // UPDATE_PAGE_H
