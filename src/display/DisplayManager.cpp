#include "DisplayManager.h"
#include "../utils/TimeUtils.h"
#include "../utils/gfxlatin2.h"
#include <WiFi.h>
#include <Arduino.h>

// Font references from src/fonts/ directory
#include "../fonts/DepartureMono4pt8b.h"
#include "../fonts/DepartureMono5pt8b.h"
#include "../fonts/DepartureMonoCondensed5pt8b.h"
#include "../fonts/DepartureWeather4pt8b.h"

// Weather API for data structure
#include "../api/WeatherAPI.h"

DisplayManager::DisplayManager()
    : display(nullptr), isDrawing(false), config(nullptr),
      lastScrollTick(0), currentDepartures(nullptr),
      currentDepartureCount(0), currentNumToDisplay(0)
{
    fontSmall = &DepartureMono_Regular4pt8b;
    fontMedium = &DepartureMono_Regular5pt8b;
    fontCondensed = &DepartureMono_Condensed5pt8b;
    fontWeather = &DepartureWeather_Regular4pt8b; // TODO: Change to &DepartureWeather4pt8b when font is created
    weatherData = nullptr;

    // Initialize scroll state for all rows
    for (int i = 0; i < 3; i++)
    {
        scrollState[i].offset = 0;
        scrollState[i].maxOffset = 0;
        scrollState[i].needsScroll = false;
        scrollState[i].paused = true;  // Start paused
        scrollState[i].atStart = true; // At beginning
        scrollState[i].lastUpdate = 0;
        scrollState[i].cycleCount = 0;
    }
}

DisplayManager::~DisplayManager()
{
    if (display)
    {
        delete display;
        display = nullptr;
    }
}

bool DisplayManager::begin(int brightness)
{
    HUB75_I2S_CFG::i2s_pins _pins = {
        R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN,
        A_PIN, B_PIN, C_PIN, D_PIN, E_PIN,
        LAT_PIN, OE_PIN, CLK_PIN};

    HUB75_I2S_CFG mxconfig(
        PANEL_WIDTH,
        PANEL_HEIGHT,
        PANELS_NUMBER,
        _pins);

    mxconfig.clkphase = false;
    mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;

    display = new MatrixPanel_I2S_DMA(mxconfig);

    if (!display->begin())
    {
        Serial.println("Display FAILED!");
        return false;
    }

    display->setBrightness8(brightness);
    display->clearScreen();

    // Initialize color constants
    initColors(display);

    return true;
}

void DisplayManager::setBrightness(int brightness)
{
    if (display)
    {
        display->setBrightness8(brightness);
    }
}

// Match platform value or stop ID against platformSymbolMap config string
// Returns direction digit ('1'-'8') or '\0' if no match
static char getPlatformSymbol(const char* platform, const char* stopId, const char* symbolMap)
{
    if (!symbolMap || symbolMap[0] == '\0')
        return '\0';

    char mapCopy[256];
    strlcpy(mapCopy, symbolMap, sizeof(mapCopy));

    // Pass 1: Check platform value matches (non-ID: prefixed entries)
    if (platform && platform[0] != '\0')
    {
        char* token = strtok(mapCopy, ",");
        while (token != nullptr)
        {
            char* equals = strchr(token, '=');
            if (equals)
            {
                *equals = '\0';
                const char* key = token;
                const char* value = equals + 1;

                // Skip ID: prefixed entries in first pass
                if (strncmp(key, "ID:", 3) != 0)
                {
                    if (strcasecmp(platform, key) == 0)
                    {
                        if (value[0] >= '1' && value[0] <= '8')
                            return value[0];
                    }
                }
            }
            token = strtok(nullptr, ",");
        }
    }

    // Pass 2: Check stop ID matches (ID: prefixed entries)
    if (stopId && stopId[0] != '\0')
    {
        strlcpy(mapCopy, symbolMap, sizeof(mapCopy));
        char* token = strtok(mapCopy, ",");
        while (token != nullptr)
        {
            char* equals = strchr(token, '=');
            if (equals)
            {
                *equals = '\0';
                const char* key = token;
                const char* value = equals + 1;

                if (strncmp(key, "ID:", 3) == 0)
                {
                    if (strcasecmp(stopId, key + 3) == 0)
                    {
                        if (value[0] >= '1' && value[0] <= '8')
                            return value[0];
                    }
                }
            }
            token = strtok(nullptr, ",");
        }
    }

    return '\0';
}

DestLayout DisplayManager::calcDestLayout(const Departure &dep)
{
    DestLayout layout;
    bool multiTimes = config && config->showMultipleTimes;
    bool platformEnabled = config && config->showPlatform;

    // Starting X position for destination text
    // Tighter gap when both dual ETA and platform are enabled to reclaim destination space
    layout.destX = (multiTimes && platformEnabled) ? 20 : 22;
    if (dep.hasAC)
    {
        layout.destX += 6;
    }

    // Check for platform symbol override
    layout.symbolChar = '\0';
    if (platformEnabled && config->platformSymbolMap[0] != '\0')
    {
        layout.symbolChar = getPlatformSymbol(dep.platform, dep.sourceStopId, config->platformSymbolMap);
    }

    // Calculate space reserved for platform (if enabled and present)
    layout.platformReservedPx = 0;
    layout.willShowPlatform = false;
    if (layout.symbolChar != '\0')
    {
        layout.willShowPlatform = true;
        layout.platformReservedPx = 7; // 5px glyph + buffer
    }
    else if (platformEnabled && dep.platform[0] != '\0')
    {
        layout.willShowPlatform = true;
        // Dynamic reservation based on platform length:
        // 1 char: medium font (6px) + 1px buffer = 7px
        // 2 chars: condensed font (8px) + 1px buffer = 9px
        // 3 chars: condensed font (12px) + 1px buffer = 13px
        int platformLen = utf8len(dep.platform);
        if (platformLen >= 3)
            layout.platformReservedPx = 13;
        else if (platformLen == 2)
            layout.platformReservedPx = 9;
        else
            layout.platformReservedPx = 7;
    }

    // Show absolute departure time (HH:MM) instead of ETA when > 60 minutes away
    layout.showAbsoluteTime = (dep.eta > 60);

    // When showMultipleTimes is off, add 2px breathing room between platform and ETA area
    if (layout.willShowPlatform && !multiTimes)
        layout.platformReservedPx += 2;

    // Determine if this row should use dual ETA layout (suppress when showing absolute time)
    layout.dualEta = multiTimes && dep.secondEta >= 0 && !layout.showAbsoluteTime;

    // Calculate right-side ETA area width based on display mode
    // Dual ETA: secondary in fontCondensed (12px, right edge X=108)
    //         + primary in fontMedium (18px, right edge X=127) = 30px
    // Absolute time: "HH:MM" in fontCondensed (17px rendered + 1px clearance = 18px)
    // Single ETA: fontMedium only — wide (>=10 or <1): 3 chars = 17px, narrow (1-9): 2 chars = 11px
    int etaAreaWidth;
    if (layout.dualEta)
        etaAreaWidth = 30;
    else if (layout.showAbsoluteTime)
        etaAreaWidth = multiTimes ? 30 : 18;
    else
        etaAreaWidth = (dep.eta >= 10 || dep.eta < 1) ? 17 : 11;

    // When showMultipleTimes is on globally, always reserve dual-ETA width
    // so destination boundary matches the platform anchor position
    int effectiveEtaArea = (multiTimes && layout.platformReservedPx > 0) ? 30 : etaAreaWidth;
    layout.spaceCalcEta = 128 - effectiveEtaArea;
    int availableSpace = layout.spaceCalcEta - layout.destX - layout.platformReservedPx;

    // Font selection based on destination length and available space
    int destLen = utf8len(dep.destination);

    // Thresholds: fontMedium @ 6px/char, fontCondensed @ 4px/char
    // Tighter threshold when right side has more info (dual ETA, platform, absolute time)
    int mediumThreshold;
    if (layout.willShowPlatform && multiTimes)
        mediumThreshold = 11;  // platform anchored at 97px: least space
    else if (layout.willShowPlatform || layout.dualEta || (layout.showAbsoluteTime && multiTimes))
        mediumThreshold = 12;  // platform at 109px, or dual ETA, or absolute time with 30px reservation
    else
        mediumThreshold = 14;  // primary ETA only
    mediumThreshold -= dep.hasAC ? 1 : 0;

    if (destLen <= mediumThreshold)
    {
        layout.font = fontMedium;
        layout.maxChars = availableSpace / 6; // 6px per char
    }
    else
    {
        layout.font = fontCondensed;
        layout.maxChars = availableSpace / 4 - 1; // 4px per char, -1 for padding
    }
    // Reclaim 1 condensed char in the tightest layout (dual ETA + platform, no AC)
    if (layout.dualEta && layout.willShowPlatform && !dep.hasAC)
      layout.maxChars += 1;

    // Safety cap: prevent buffer overflow
    if (layout.maxChars > 63)
        layout.maxChars = 63; // destTrunc buffer size - 1
    if (layout.maxChars < 1)
        layout.maxChars = 1; // Ensure at least 1 char

    return layout;
}

void DisplayManager::drawDeparture(int row, const Departure &dep)
{
    int y = row * 8; // Each row is 8 pixels

    // Convert line number and destination to ISO-8859-2 (in-place)
    char lineConverted[8];
    char destConverted[64];
    strlcpy(lineConverted, dep.line, sizeof(lineConverted));
    strlcpy(destConverted, dep.destination, sizeof(destConverted));
    utf8tocp(lineConverted);
    utf8tocp(destConverted);

    // Calculate destination layout (font, maxChars, platform reservation, etc.)
    DestLayout layout = calcDestLayout(dep);

    // Draw line number background - always black (fixed width for all routes)
    uint16_t lineColor = getLineColorWithConfig(dep.line, config ? config->lineColorMap : "");
    int bgWidth = 18; // Fixed width to fit up to 4 characters
    display->fillRect(1, y + 1, bgWidth, 7, COLOR_BLACK);

    // Line number text - colored text on black background
    display->setTextColor(lineColor);

    // Select font based on line number length
    // 1-3 characters: medium font (6px/char)
    // 4 characters: condensed font (4px/char)
    int lineLen = strlen(lineConverted);
    const GFXfont *lineFont = (lineLen >= 4) ? fontCondensed : fontMedium;
    display->setFont(lineFont);

    // Center the line number text within the background rectangle
    int16_t x1, y1;
    uint16_t w, h;
    display->getTextBounds(lineConverted, 0, 0, &x1, &y1, &w, &h);
    // Account for font's left bearing offset (x1) when centering
    int textX = 1 + (bgWidth - w) / 2 - x1;
    // Align baseline with destination (y + 7)
    display->setCursor(textX, y + 7);
    display->print(lineConverted);

    // AC indicator (asterisk before destination)
    if (dep.hasAC)
    {
        int acX = (config && config->showMultipleTimes && config->showPlatform) ? 20 : 22;
        display->setTextColor(COLOR_CYAN);
        display->setCursor(acX, y + 7);
        display->print("*");
    }

    // Destination text
    display->setTextColor(COLOR_WHITE);
    display->setFont(layout.font);
    display->setCursor(layout.destX, y + 7);

    // Check if scrolling is needed for this row (only if enabled in config)
    int destLen = strlen(destConverted);
    bool needsScroll = (config && config->scrollEnabled) && (destLen > layout.maxChars);
    char destTrunc[64];

    if (needsScroll && row < 3)
    {
        // Set up scroll state for this row
        scrollState[row].needsScroll = true;
        scrollState[row].maxOffset = destLen - layout.maxChars;

        // Apply current scroll offset
        int scrollOffset = scrollState[row].offset;
        if (scrollOffset > scrollState[row].maxOffset)
        {
            scrollOffset = scrollState[row].maxOffset;
        }

        // Copy substring starting at scroll offset
        strlcpy(destTrunc, destConverted + scrollOffset, layout.maxChars + 1);
    }
    else
    {
        // No scrolling needed - reset state and show full text
        if (row < 3)
        {
            scrollState[row].needsScroll = false;
            scrollState[row].offset = 0;
        }
        strlcpy(destTrunc, destConverted, layout.maxChars + 1);
    }
    display->print(destTrunc);

    // Platform display (if enabled and present)
    if (layout.willShowPlatform)
    {
        if (layout.symbolChar != '\0')
        {
            // Render directional arrow using weather font
            display->setFont(fontWeather);
            int platformAnchor = config && config->showMultipleTimes ? 97 : 109;

            int16_t px1, py1;
            uint16_t pw, ph;
            char symBuf[2] = {layout.symbolChar, '\0'};
            display->getTextBounds(symBuf, 0, 0, &px1, &py1, &pw, &ph);
            int platformX = platformAnchor - pw - 1 - px1;

            display->setTextColor(COLOR_CYAN);
            display->setCursor(platformX, y + 7);
            display->print(symBuf);
        }
        else
        {
            // Original text rendering for platform
            char platformConverted[8];
            strlcpy(platformConverted, dep.platform, sizeof(platformConverted));
            utf8tocp(platformConverted);

            // Safety truncation to 3 characters
            if (strlen(platformConverted) > 3)
            {
                platformConverted[3] = '\0';
            }

            // Select font based on platform length (same logic as line numbers)
            int platformLen = strlen(platformConverted);
            const GFXfont *platformFont = (platformLen >= 2) ? fontCondensed : fontMedium;
            display->setFont(platformFont);

            // Get actual text bounds for proper alignment (like line number centering)
            int16_t px1, py1;
            uint16_t pw, ph;
            display->getTextBounds(platformConverted, 0, 0, &px1, &py1, &pw, &ph);

            // Position: right-align to ETA position anchor
            // Dual ETA: anchor at 98px (shifted left for mixed-font ETAs), normal: 111px
            int platformAnchor = config && config->showMultipleTimes ? 97 : 109;
            int platformX = platformAnchor - pw - 1 - px1;

            display->setTextColor(COLOR_CYAN); // Match AC indicator
            display->setCursor(platformX, y + 7);
            display->print(platformConverted);
        }
    }

    // ETA urgency color based on minutes until departure
    auto etaColor = [&](int eta) -> uint16_t {
        if (dep.isDelayed && dep.delayMinutes > 0)
            return COLOR_ORANGE;
        if (eta < 2)
            return COLOR_RED;
        if (eta < 5)
            return COLOR_YELLOW;
        return COLOR_WHITE;
    };

    // Right-align text to an anchor X position, measure and draw
    auto drawRightAligned = [&](const char *text, int anchorX) {
        int16_t tx1, ty1;
        uint16_t tw, th;
        display->getTextBounds(text, 0, 0, &tx1, &ty1, &tw, &th);
        display->setCursor(anchorX - tw - tx1, y + 7);
        display->print(text);
    };

    // Format ETA minutes into buffer
    auto formatEtaMinutes = [](char *buf, size_t sz, int eta) {
        if (eta < 1)
            snprintf(buf, sz, "<1'");
        else
            snprintf(buf, sz, "%d'", eta);
    };

    char etaText[8];

    if (layout.dualEta)
    {
        // Secondary ETA — fontCondensed, dim gray, right edge at X=109
        display->setFont(fontCondensed);

        char eta2Text[8];
        if (dep.secondEta < 1)
            snprintf(eta2Text, sizeof(eta2Text), "<1'");
        else if (dep.secondEta >= 100)
            snprintf(eta2Text, sizeof(eta2Text), ">1h");
        else
            snprintf(eta2Text, sizeof(eta2Text), "%d'", dep.secondEta);

        display->setTextColor(display->color565(90, 90, 90));
        drawRightAligned(eta2Text, 109);

        // Primary ETA — fontMedium, urgency-colored, right edge at X=127
        display->setFont(fontMedium);
        formatEtaMinutes(etaText, sizeof(etaText), dep.eta);
        display->setTextColor(etaColor(dep.eta));
        drawRightAligned(etaText, 127);
    }
    else if (layout.showAbsoluteTime)
    {
        // Absolute departure time — fontCondensed, white, right edge at X=127
        display->setFont(fontCondensed);
        struct tm *t = localtime(&dep.departureTime);
        snprintf(etaText, sizeof(etaText), "%02d:%02d", t->tm_hour, t->tm_min);
        display->setTextColor(COLOR_WHITE);
        drawRightAligned(etaText, 127);
    }
    else
    {
        // Single ETA — fontMedium, urgency-colored, right edge at X=127
        display->setFont(fontMedium);
        formatEtaMinutes(etaText, sizeof(etaText), dep.eta);
        display->setTextColor(etaColor(dep.eta));
        drawRightAligned(etaText, 127);
    }
}

void DisplayManager::drawDateTime()
{
    int y = 24; // Bottom row

    struct tm timeinfo;
    if (!getCurrentTime(&timeinfo))
    {
        display->setTextColor(COLOR_RED);
        display->setFont(fontSmall);
        display->setCursor(2, y + 7);
        display->print("Time Sync...");
        return;
    }

    display->setFont(fontSmall);
    display->setTextColor(COLOR_WHITE);

    // Get language setting (default to "en" if config not set)
    const char *lang = (config && config->language[0]) ? config->language : "en";

    // Date first (numeric format: DD.MM.) - fixed width
    char dateStr[8];
    snprintf(dateStr, sizeof(dateStr), "%02d.%02d.", timeinfo.tm_mday, timeinfo.tm_mon + 1);
    display->setCursor(2, y + 7);
    display->print(dateStr);

    // Day of week (full name, localized) - variable width
    char dayStr[16]; // Buffer for "Donnerstag" (10 chars + accents)
    const char *localDay = getLocalizedDayFull(timeinfo.tm_wday, lang);
    snprintf(dayStr, sizeof(dayStr), "%s", localDay);
    utf8tocp(dayStr);
    display->setCursor(29, y + 7); // After date (6 chars × 4px = 24px + 2px margin)
    display->print(dayStr);

    // Weather (only if enabled and valid data)
    if (config && config->weatherEnabled && weatherData && !weatherData->hasError)
    {
        time_t now;
        time(&now);

        // Only show if data is fresh (< 30 min old)
        if (difftime(now, weatherData->timestamp) < 1800)
        {
            // Switch to weather font
            display->setFont(fontWeather); // DepartureWeather4pt8b

            // Get icon character and color for this weather code
            char iconCode = mapWeatherCodeToIcon(weatherData->weatherCode);
            uint16_t iconColor = getWeatherColor(weatherData->weatherCode);

            // Draw icon at X=75 (shifted +10px from original X=65)
            display->setTextColor(iconColor);
            display->setCursor(75, y + 7);
            display->print(iconCode); // Letter 'a'-'t' renders as weather icon

            // Draw temperature left-aligned after weather icon
            display->setFont(fontSmall);
            display->setTextColor(getTemperatureColor(weatherData->temperature));
            char tempStr[8];
            snprintf(tempStr, sizeof(tempStr), "%d\xB0", weatherData->temperature);

            display->setCursor(86, y + 7);
            display->print(tempStr); // Temperature with degree symbol

            // Switch back to small font for time display
            display->setFont(fontSmall);
            display->setTextColor(COLOR_WHITE);
        }
    }

    // Time
    display->setFont(fontSmall);
    display->setTextColor(COLOR_WHITE);
    char timeStr[6];
    strftime(timeStr, 6, "%H:%M", &timeinfo);
    display->setCursor(102, y + 7);
    display->print(timeStr);
}

void DisplayManager::drawStatus(const char *line1, const char *line2, uint16_t color)
{
    display->clearScreen();
    display->setTextColor(color);
    display->setFont(fontMedium);

    if (line1)
    {
        display->setCursor(2, 12);
        display->print(line1);
    }
    if (line2)
    {
        display->setCursor(2, 24);
        display->print(line2);
    }
}

void DisplayManager::drawOTAProgress(size_t progress, size_t total)
{
    if (isDrawing)
        return;

    isDrawing = true;

    display->clearScreen();

    // Title
    display->setFont(fontMedium);
    display->setTextColor(COLOR_CYAN);
    display->setCursor(2, 8);
    display->print("Uploading...");

    // Calculate percentage
    int percentage = 0;
    if (total > 0)
    {
        percentage = (progress * 100) / total;
        if (percentage > 100)
            percentage = 100;
    }

    // Draw progress bar (center of display)
    int barWidth = 120; // Total bar width
    int barHeight = 10;
    int barX = 4;  // 4px left margin
    int barY = 13; // Center vertically

    // Draw border
    display->drawRect(barX, barY, barWidth, barHeight, COLOR_WHITE);

    // Fill progress
    int fillWidth = ((barWidth - 2) * percentage) / 100;
    if (fillWidth > 0)
    {
        display->fillRect(barX + 1, barY + 1, fillWidth, barHeight - 2, COLOR_CYAN);
    }

    // Display percentage text
    display->setFont(fontMedium);
    display->setTextColor(COLOR_WHITE);
    char percentStr[8];
    snprintf(percentStr, sizeof(percentStr), "%d%%", percentage);

    // Center the percentage text at the bottom
    int16_t x1, y1;
    uint16_t w, h;
    display->getTextBounds(percentStr, 0, 0, &x1, &y1, &w, &h);
    int textX = (128 - w) / 2 - x1;

    display->setCursor(textX, 31);
    display->print(percentStr);

    isDrawing = false;
}

void DisplayManager::drawAPMode(const char *ssid, const char *password)
{
    display->setFont(fontSmall);

    // Title
    display->setTextColor(COLOR_CYAN);
    display->setCursor(2, 7);
    display->print("WiFi Setup Mode");

    // SSID
    display->setTextColor(COLOR_WHITE);
    display->setCursor(2, 15);
    display->print("SSID:");
    display->setTextColor(COLOR_YELLOW);
    display->setCursor(32, 15);
    display->print(ssid);

    // Password
    display->setTextColor(COLOR_WHITE);
    display->setCursor(2, 23);
    display->print("Pass:");
    display->setTextColor(COLOR_GREEN);
    display->setCursor(32, 23);
    display->print(password);

    // IP
    display->setTextColor(COLOR_WHITE);
    display->setCursor(2, 31);
    display->print("Go to: 192.168.4.1");
}

// ============================================================================
// Pure Rendering Methods (called by DisplayController)
// ============================================================================

void DisplayManager::drawDepartures(const Departure *departures, int departureCount, int numToDisplay)
{
    if (isDrawing)
        return;

    // Check if departure data has changed - reset scroll if so
    bool dataChanged = (departures != currentDepartures) ||
                       (departureCount != currentDepartureCount) ||
                       (numToDisplay != currentNumToDisplay);

    // Store current departures reference for scroll updates
    currentDepartures = departures;
    currentDepartureCount = departureCount;
    currentNumToDisplay = numToDisplay;

    // Reset scroll state when data changes
    if (dataChanged)
    {
        resetScroll();
    }

    isDrawing = true;
    display->clearScreen();
    delay(1);

    // Draw departures (top 3 rows, or fewer if numToDisplay is less)
    // When showMultipleTimes is on, skip duplicates (same line+destination
    // already shown on a previous row with its secondEta).
    int maxRows = (numToDisplay < 3) ? numToDisplay : 3;

    int row = 0;
    for (int i = 0; i < departureCount && row < maxRows; i++)
    {
        if (config && config->showMultipleTimes)
        {
            bool isDuplicate = false;
            for (int r = 0; r < row; r++)
            {
                if (strcmp(renderedDeps[r].line, departures[i].line) == 0 &&
                    strcmp(renderedDeps[r].destination, departures[i].destination) == 0)
                {
                    isDuplicate = true;
                    break;
                }
            }
            if (isDuplicate) continue;
        }

        renderedDeps[row] = departures[i];
        drawDeparture(row, departures[i]);
        delay(1);
        row++;
    }

    drawDateTime();
    delay(1);

    isDrawing = false;
}

void DisplayManager::clearDisplay()
{
    if (isDrawing)
        return;

    isDrawing = true;
    display->clearScreen();
    display->setBrightness8(0); // Turn off display
    delay(1);
    isDrawing = false;
}

// ============================================================================
// Legacy Method (deprecated - use DisplayController instead)
// ============================================================================

void DisplayManager::updateDisplay(const Departure *departures, int departureCount, int numToDisplay,
                                   bool wifiConnected, bool apModeActive,
                                   const char *apSSID, const char *apPassword,
                                   bool apiError, const char *apiErrorMsg,
                                   const char *stopName, bool apiKeyConfigured,
                                   bool demoModeActive)
{
    // DEPRECATED: This method kept for backward compatibility during refactoring
    // New code should use DisplayController::render() instead
    //
    // This method now just calls drawDepartures() - state logic moved to DisplayController

    if (isDrawing)
        return;

    // Check if departure data has changed - reset scroll if so
    bool dataChanged = (departures != currentDepartures) ||
                       (departureCount != currentDepartureCount) ||
                       (numToDisplay != currentNumToDisplay);

    // Store current departures reference for scroll updates
    currentDepartures = departures;
    currentDepartureCount = departureCount;
    currentNumToDisplay = numToDisplay;

    // Reset scroll state when data changes
    if (dataChanged)
    {
        resetScroll();
    }

    isDrawing = true;
    display->clearScreen();
    delay(1);

    // Demo mode has highest priority - bypass all status screens
    // and show demo departures regardless of WiFi/API/config state
    if (demoModeActive)
    {
        // Draw demo departures directly
        int rowsToDraw = (departureCount < numToDisplay) ? departureCount : numToDisplay;
        if (rowsToDraw > 3)
            rowsToDraw = 3; // Maximum 3 rows on display

        for (int i = 0; i < rowsToDraw; i++)
        {
            renderedDeps[i] = departures[i];
            drawDeparture(i, departures[i]);
            delay(1);
        }

        drawDateTime();
        delay(1);

        isDrawing = false;
        return;
    }

    // AP Mode - Show credentials
    if (apModeActive)
    {
        drawAPMode(apSSID, apPassword);
        isDrawing = false;
        return;
    }

    if (!wifiConnected)
    {
        // We don't have access to config here, so just show generic message
        drawStatus("WiFi Connecting...", "", COLOR_YELLOW);
        isDrawing = false;
        return;
    }

    if (!apiKeyConfigured)
    {
        char ipStr[32];
        snprintf(ipStr, sizeof(ipStr), "http://%s", WiFi.localIP().toString().c_str());
        drawStatus("Setup Required", ipStr, COLOR_CYAN);
        isDrawing = false;
        return;
    }

    if (apiError)
    {
        drawStatus("API Error", apiErrorMsg, COLOR_RED);
        drawDateTime();
        isDrawing = false;
        return;
    }

    if (departureCount == 0)
    {
        drawStatus("No Departures", stopName[0] ? stopName : "Waiting...", COLOR_YELLOW);
        drawDateTime();
        isDrawing = false;
        return;
    }

    // Draw departures (top 3 rows, or fewer if numToDisplay is less)
    int rowsToDraw = (departureCount < numToDisplay) ? departureCount : numToDisplay;
    if (rowsToDraw > 3)
        rowsToDraw = 3; // Maximum 3 rows on display

    for (int i = 0; i < rowsToDraw; i++)
    {
        renderedDeps[i] = departures[i];
        drawDeparture(i, departures[i]);
        delay(1);
    }

    drawDateTime();
    delay(1);

    isDrawing = false;
}

void DisplayManager::drawDemo(const Departure *departures, int departureCount, const char *stopName)
{
    if (isDrawing)
        return;

    isDrawing = true;
    display->clearScreen();
    delay(1);

    // Draw sample departures (top 1-3 rows)
    int rowsToDraw = (departureCount < 3) ? departureCount : 3;
    for (int i = 0; i < rowsToDraw; i++)
    {
        renderedDeps[i] = departures[i];
        drawDeparture(i, departures[i]);
        delay(1);
    }

    // Draw date/time status bar
    drawDateTime();
    delay(1);

    isDrawing = false;
}

void DisplayManager::resetScroll()
{
    for (int i = 0; i < 3; i++)
    {
        scrollState[i].offset = 0;
        scrollState[i].maxOffset = 0;
        scrollState[i].needsScroll = false;
        scrollState[i].paused = true;  // Start paused
        scrollState[i].atStart = true; // At beginning
        scrollState[i].lastUpdate = millis();
        scrollState[i].cycleCount = 0;
    }
    lastScrollTick = millis();
}

void DisplayManager::clearScreen()
{
    if (display == nullptr || isDrawing)
    {
        return;
    }

    isDrawing = true;
    display->clearScreen();
    display->flipDMABuffer();
    isDrawing = false;
}

bool DisplayManager::updateScroll()
{
    // Don't update if we're in the middle of a full redraw
    if (isDrawing || !display)
        return false;

    // Check if we have departure data to scroll
    if (!currentDepartures || currentDepartureCount == 0)
        return false;

    unsigned long now = millis();

    // Determine how many rows to consider
    int rowsToCheck = (currentDepartureCount < currentNumToDisplay) ? currentDepartureCount : currentNumToDisplay;
    if (rowsToCheck > 3)
        rowsToCheck = 3;

    // Process only ONE row per call - round-robin through rows
    // Use lastScrollTick to track which row to check next
    static int currentRow = 0;

    // Find next row that needs scrolling (up to rowsToCheck attempts)
    for (int attempts = 0; attempts < rowsToCheck; attempts++)
    {
        int row = currentRow;
        currentRow = (currentRow + 1) % rowsToCheck; // Move to next row for next call

        if (!scrollState[row].needsScroll)
            continue;

        // Stop scrolling if we've hit the max cycle count
        if (scrollState[row].cycleCount >= SCROLL_MAX_CYCLES)
            continue;

        // Check if we're in a pause period
        if (scrollState[row].paused)
        {
            // Determine pause duration based on position
            int pauseDuration = scrollState[row].atStart ? SCROLL_PAUSE_START_MS : SCROLL_PAUSE_END_MS;

            // Wait for pause duration to expire
            if (now - scrollState[row].lastUpdate >= (unsigned long)pauseDuration)
            {
                // Pause is over
                scrollState[row].paused = false;
                scrollState[row].lastUpdate = now;

                // If we were at the end, reset to beginning and increment cycle count
                if (!scrollState[row].atStart)
                {
                    scrollState[row].cycleCount++;

                    // Always reset to start position after finishing a cycle
                    scrollState[row].offset = 0;
                    scrollState[row].paused = true; // Pause at start (forever if max cycles hit)
                    scrollState[row].atStart = true;

                    // Redraw at position 0
                    redrawDestination(row, renderedDeps[row]);
                    return true; // Only process one row per call
                }
            }
            continue; // Still paused, try next row
        }

        // Check if it's time for the next scroll step
        if (now - scrollState[row].lastUpdate >= SCROLL_INTERVAL_MS)
        {
            scrollState[row].lastUpdate = now;

            // Increment offset
            scrollState[row].offset++;

            // Check if we've reached the end
            if (scrollState[row].offset >= scrollState[row].maxOffset)
            {
                // At end - pause, then reset to beginning
                scrollState[row].offset = scrollState[row].maxOffset;
                scrollState[row].paused = true;
                scrollState[row].atStart = false; // At end
            }

            // Redraw just this row's destination
            redrawDestination(row, renderedDeps[row]);
            return true; // Only process one row per call
        }
    }

    return false;
}

void DisplayManager::redrawDestination(int row, const Departure &dep)
{
    if (row < 0 || row >= 3 || !display)
        return;

    int y = row * 8;

    // Convert destination to ISO-8859-2
    char destConverted[64];
    strlcpy(destConverted, dep.destination, sizeof(destConverted));
    utf8tocp(destConverted);

    // Use shared layout calculation
    DestLayout layout = calcDestLayout(dep);

    // Clear the destination area (from destX to just before platform/ETA)
    int clearWidth = layout.spaceCalcEta - layout.destX - layout.platformReservedPx;
    display->fillRect(layout.destX, y, clearWidth, 8, COLOR_BLACK);

    // Apply scroll offset and draw
    display->setFont(layout.font);
    display->setTextColor(COLOR_WHITE);
    display->setCursor(layout.destX, y + 7);

    char destTrunc[64];
    int scrollOffset = scrollState[row].offset;
    if (scrollOffset > scrollState[row].maxOffset)
    {
        scrollOffset = scrollState[row].maxOffset;
    }
    strlcpy(destTrunc, destConverted + scrollOffset, layout.maxChars + 1);
    display->print(destTrunc);
}

// ============================================================================
// Ticker Mode: Candlestick Chart
// ============================================================================

void DisplayManager::drawTicker(const TickerData& ticker)
{
    if (isDrawing || !display)
        return;

    isDrawing = true;
    display->clearScreen();
    delay(1);

    // Colors for candles
    uint16_t colorGreen = display->color565(0, 200, 0);
    uint16_t colorRed = display->color565(200, 0, 0);

    // Chart area: rows 0-2 = 24 pixels tall (y: 0-23)
    const int chartHeight = 24;
    const int chartTop = 0;

    // Format price text to measure its width
    char priceText[16];
    if (ticker.currentPrice >= 1000.0f)
    {
        int intPrice = (int)ticker.currentPrice;
        // Format with comma separator (e.g., "59,137")
        if (intPrice >= 1000000)
            snprintf(priceText, sizeof(priceText), "%d,%03d,%03d", intPrice / 1000000, (intPrice / 1000) % 1000, intPrice % 1000);
        else if (intPrice >= 1000)
            snprintf(priceText, sizeof(priceText), "%d,%03d", intPrice / 1000, intPrice % 1000);
        else
            snprintf(priceText, sizeof(priceText), "%d", intPrice);
    }
    else if (ticker.currentPrice >= 1.0f)
    {
        snprintf(priceText, sizeof(priceText), "%.2f", ticker.currentPrice);
    }
    else
    {
        snprintf(priceText, sizeof(priceText), "%.4f", ticker.currentPrice);
    }

    // Trend arrow: rendered separately in weather font ('2' = up, '4' = down)
    bool trendUp = (ticker.currentPrice >= ticker.previousClose);
    char arrowChar = trendUp ? '2' : '4';
    const int arrowWidth = 5;   // Weather font arrow is 5px wide
    const int arrowPadLeft = 1; // Weather font arrows have no left padding, add 1px

    // Measure price text width using condensed font
    display->setFont(fontCondensed);
    int16_t px1, py1;
    uint16_t pw, ph;
    display->getTextBounds(priceText, 0, 0, &px1, &py1, &pw, &ph);

    // Total price+arrow width
    int priceWithArrowWidth = (int)pw + arrowPadLeft + arrowWidth;

    // Measure symbol text width using small font
    display->setFont(fontSmall);
    int16_t sx1, sy1;
    uint16_t sw, sh;
    char symbolTrunc[10];
    strlcpy(symbolTrunc, ticker.symbol, sizeof(symbolTrunc));
    display->getTextBounds(symbolTrunc, 0, 0, &sx1, &sy1, &sw, &sh);

    // Right-side text area = widest of price+arrow or symbol, plus 2px gap from chart
    int textWidth = (priceWithArrowWidth > (int)sw) ? priceWithArrowWidth : (int)sw;
    int priceAreaWidth = textWidth + 3; // 2px gap + 1px padding from right edge
    int chartWidth = 128 - priceAreaWidth;

    // Number of candles that fit (4px per candle: 3px body + 1px gap)
    int candlesVisible = chartWidth / 4;
    if (candlesVisible > ticker.candleCount)
        candlesVisible = ticker.candleCount;
    if (candlesVisible < 1)
        candlesVisible = 1;

    // Offset into candle array (show rightmost/newest candles)
    int startCandle = ticker.candleCount - candlesVisible;
    if (startCandle < 0)
        startCandle = 0;

    // Find price range for Y-axis scaling
    float minLow = ticker.candles[startCandle].low;
    float maxHigh = ticker.candles[startCandle].high;
    for (int i = startCandle; i < startCandle + candlesVisible; i++)
    {
        if (ticker.candles[i].low < minLow)
            minLow = ticker.candles[i].low;
        if (ticker.candles[i].high > maxHigh)
            maxHigh = ticker.candles[i].high;
    }

    float priceRange = maxHigh - minLow;
    if (priceRange < 0.0001f)
        priceRange = 0.0001f; // Prevent division by zero

    // Draw each candle
    for (int i = 0; i < candlesVisible; i++)
    {
        const TickerCandle& c = ticker.candles[startCandle + i];
        int x = i * 4; // 4px per candle slot (3px body + 1px gap)

        bool bullish = (c.close >= c.open);
        uint16_t candleColor = bullish ? colorGreen : colorRed;

        // Map prices to pixel Y coordinates (inverted: high price = low Y)
        int yHigh = chartTop + (int)((maxHigh - c.high) / priceRange * (chartHeight - 1));
        int yLow = chartTop + (int)((maxHigh - c.low) / priceRange * (chartHeight - 1));
        int yOpen = chartTop + (int)((maxHigh - c.open) / priceRange * (chartHeight - 1));
        int yClose = chartTop + (int)((maxHigh - c.close) / priceRange * (chartHeight - 1));

        // Clamp to chart area
        yHigh = constrain(yHigh, chartTop, chartTop + chartHeight - 1);
        yLow = constrain(yLow, chartTop, chartTop + chartHeight - 1);
        yOpen = constrain(yOpen, chartTop, chartTop + chartHeight - 1);
        yClose = constrain(yClose, chartTop, chartTop + chartHeight - 1);

        // Draw wick (1px wide center line from high to low)
        int wickX = x + 1; // Center of 3px body
        display->drawFastVLine(wickX, yHigh, yLow - yHigh + 1, candleColor);

        // Draw body (3px wide from open to close)
        int bodyTop = bullish ? yClose : yOpen;
        int bodyBot = bullish ? yOpen : yClose;
        int bodyHeight = bodyBot - bodyTop + 1;
        if (bodyHeight < 1)
            bodyHeight = 1; // Doji: minimum 1px

        display->fillRect(x, bodyTop, 3, bodyHeight, candleColor);
    }

    // Draw text right-aligned: anchor from right edge, calculate X positions backward
    int rightEdge = 127; // Rightmost pixel

    // Price + arrow layout: [price text][1px pad][arrow 5px] right-aligned
    int arrowX = rightEdge - arrowWidth + 1;
    int priceX = arrowX - arrowPadLeft - (int)pw - px1;
    int priceY = chartTop + chartHeight / 2 + 3; // Approximate vertical center baseline

    uint16_t priceColor = trendUp ? colorGreen : colorRed;

    // Draw price text
    display->setFont(fontCondensed);
    display->setTextColor(priceColor);
    display->setCursor(priceX, priceY);
    display->print(priceText);

    // Draw trend arrow in weather font
    char arrowStr[2] = {arrowChar, '\0'};
    display->setFont(fontWeather);
    display->setTextColor(priceColor);
    display->setCursor(arrowX, priceY);
    display->print(arrowStr);

    // Draw symbol name above price in small font (also right-aligned)
    int symbolX = rightEdge - (int)sw - sx1 + 1;
    int symbolY = priceY - 9; // Above price
    if (symbolY < 6)
        symbolY = 6;
    display->setFont(fontSmall);
    display->setTextColor(COLOR_WHITE);
    display->setCursor(symbolX, symbolY);
    display->print(symbolTrunc);

    // Draw status bar (row 3)
    drawDateTime();
    delay(1);

    isDrawing = false;
}

// ============================================================================
// Weather Helper Functions
// ============================================================================

char DisplayManager::mapWeatherCodeToIcon(int wmoCode)
{
    // Map WMO weather codes to weather font characters
    // WMO codes: https://www.noaa.gov/weather
    if (wmoCode == 0)
        return 'a'; // Clear sky → sun
    if (wmoCode <= 3)
        return 'b'; // Partly cloudy/cloudy
    if (wmoCode >= 45 && wmoCode <= 48)
        return 'f'; // Fog
    if (wmoCode >= 51 && wmoCode <= 57)
        return 'g'; // Drizzle/light rain
    if (wmoCode >= 61 && wmoCode <= 67)
        return 'd'; // Rain
    if (wmoCode >= 71 && wmoCode <= 86)
        return 'e'; // Snow/sleet
    if (wmoCode >= 95)
        return 't'; // Thunderstorm
    return 'c';     // Default: cloudy
}

uint16_t DisplayManager::getWeatherColor(int wmoCode)
{
    // Color coding by weather condition
    if (wmoCode == 0)
        return COLOR_YELLOW; // Sunny
    if (wmoCode <= 3)
        return COLOR_WHITE; // Cloudy
    if (wmoCode >= 45 && wmoCode <= 48)
        return COLOR_PURPLE; // Fog
    if (wmoCode >= 51 && wmoCode <= 67)
        return COLOR_CYAN; // Drizzle/Rain
    if (wmoCode >= 71 && wmoCode <= 86)
        return COLOR_BLUE; // Snow
    if (wmoCode >= 95)
        return COLOR_RED; // Thunderstorm
    return COLOR_WHITE;   // Default
}

uint16_t DisplayManager::getTemperatureColor(int temperature)
{
    if (temperature > 25)
        return COLOR_RED; // Hot
    if (temperature > 16)
        return COLOR_YELLOW; // Warm
    if (temperature < 8)
        return COLOR_CYAN; // Cold
    return COLOR_WHITE;    // Mild
}
