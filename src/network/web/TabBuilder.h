#ifndef TABBUILDER_H
#define TABBUILDER_H

#include <WString.h>

/**
 * Build header section with title and action bar
 * @param apModeActive Whether device is in AP mode
 * @param restModeActive Whether rest mode is active. The rest button renders from
 *        THIS ALONE -- restModeManual is a label, not state, and keying the button
 *        to it made the button dead during a scheduled rest (TA-0254).
 * @return HTML string for header
 */
String buildHeader(bool apModeActive, bool restModeActive);

/**
 * Build tab navigation bar
 * @param apModeActive Whether device is in AP mode (shows 3 tabs vs 5 tabs)
 * @return HTML string for tab bar
 */
String buildTabBar(bool apModeActive);

/**
 * Build status banner section
 * @param apModeActive Whether device is in AP mode
 * @param demoModeActive Whether demo mode is active
 * @param restModeActive Whether rest mode is active
 * @param restModeManual Whether rest mode was manually triggered
 * @param hasApiError Whether API error occurred
 * @param apiErrorMsg API error message (if any)
 * @param apSsid AP mode SSID (if in AP mode)
 * @param apPassword AP mode password (if in AP mode)
 * @return HTML string for status banners
 */
String buildStatusBanner(bool apModeActive, bool demoModeActive, bool restModeActive, bool restModeManual,
                         bool hasApiError, const char* apiErrorMsg, const char* apSsid, const char* apPassword);

#endif // TABBUILDER_H
