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

    // Determine if this row should use dual ETA layout
    bool dualEta = config && config->showMultipleTimes && dep.secondEta >= 0;

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
    int destX = 22; // Fixed position for all destinations (18px max route width + 4px gap)
    if (dep.hasAC)
    {
        display->setTextColor(COLOR_CYAN);
        display->setCursor(destX, y + 7);
        display->print("*");
        destX += 6;
    }

    // Calculate space reserved for platform (if enabled and present)
    int platformReservedPx = 0;
    bool willShowPlatform = false;
    if (config && config->showPlatform && dep.platform[0] != '\0')
    {
        willShowPlatform = true;
        // Dynamic reservation based on platform length:
        // 1 char: medium font (6px) + 3px buffer = 9px
        // 2 chars: condensed font (8px) + 3px buffer = 11px
        // 3 chars: condensed font (12px) + 3px buffer = 15px
        int platformLen = strlen(dep.platform);
        if (platformLen >= 3)
        {
            platformReservedPx = 15; // 3 condensed chars + buffer
        }
        else if (platformLen == 2)
        {
            platformReservedPx = 11; // 2 condensed chars + buffer
        }
        else
        {
            platformReservedPx = 9; // 1 medium char + buffer
        }
    }

    // Destination - adaptive font based on available space
    display->setTextColor(COLOR_WHITE);

    int destLen = strlen(destConverted);

    // Calculate available space: ETA position - destination start - platform reservation
    // ETA positions: 111px (wide: >=10 or <1), 117px (narrow: 1-9)
    // Dual ETA mode: fixed at 103px to fit two condensed-font times
    int etaPosition;
    if (dualEta)
    {
        etaPosition = 103;
    }
    else
    {
        etaPosition = (dep.eta >= 10 || dep.eta < 1) ? 111 : 117;
    }

    // When platform is shown, always reserve space for widest ETA position
    // to keep platform alignment consistent across rows
    int spaceCalcEta = dualEta ? 103 : ((platformReservedPx > 0) ? 111 : etaPosition);
    int availableSpace = spaceCalcEta - destX - platformReservedPx;

    // Font selection based on destination length and available space
    const GFXfont *destFont;
    int maxChars;

    // Thresholds: fontMedium @ 6px/char, fontCondensed @ 4px/char
    // Without platform: ~89px → 14 chars (medium) or 22 chars (condensed)
    // With platform: ~74px → 12 chars (medium) or 18 chars (condensed)
    int mediumThreshold = platformReservedPx > 0 ? 12 : 14;
    mediumThreshold -= dep.hasAC ? 1 : 0;

    if (destLen <= mediumThreshold)
    {
        // Short destination - use regular font
        destFont = fontMedium;
        maxChars = availableSpace / 6; // 6px per char
    }
    else
    {
        // Long destination - use condensed font
        destFont = fontCondensed;
        maxChars = availableSpace / 4 - 1; // 4px per char, -1 for padding
    }

    // Safety cap: prevent buffer overflow
    if (maxChars > 63)
        maxChars = 63; // destTrunc buffer size - 1
    if (maxChars < 1)
        maxChars = 1; // Ensure at least 1 char

    display->setFont(destFont);
    display->setCursor(destX, y + 7);

    // Check if scrolling is needed for this row (only if enabled in config)
    bool needsScroll = (config && config->scrollEnabled) && (destLen > maxChars);
    char destTrunc[64];

    if (needsScroll && row < 3)
    {
        // Set up scroll state for this row
        scrollState[row].needsScroll = true;
        scrollState[row].maxOffset = destLen - maxChars;

        // Apply current scroll offset
        int scrollOffset = scrollState[row].offset;
        if (scrollOffset > scrollState[row].maxOffset)
        {
            scrollOffset = scrollState[row].maxOffset;
        }

        // Copy substring starting at scroll offset
        strncpy(destTrunc, destConverted + scrollOffset, maxChars);
    }
    else
    {
        // No scrolling needed - reset state and show full text
        if (row < 3)
        {
            scrollState[row].needsScroll = false;
            scrollState[row].offset = 0;
        }
        strncpy(destTrunc, destConverted, maxChars);
    }
    destTrunc[maxChars] = '\0';
    display->print(destTrunc);

    // Platform display (if enabled and present)
    if (willShowPlatform)
    {
        // Convert platform to ISO-8859-2 (in case of special characters)
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
        // Dual ETA: anchor at 103px (shifted 8px left), normal: 111px
        int platformAnchor = config && config->showMultipleTimes ? 103 : 111;
        int platformX = platformAnchor - pw - 3 - px1;

        display->setTextColor(COLOR_CYAN); // Match AC indicator
        display->setCursor(platformX, y + 7);
        display->print(platformConverted);
    }

    // ETA display — right-aligned to panel edge
    // Format ETA into buffer, measure with getTextBounds, position right edge at anchor
    int16_t tx1, ty1;
    uint16_t tw, th;

    if (dualEta)
    {
        // Dual ETA mode: two right-aligned condensed-font columns
        // First ETA right edge at 114, second at 127
        display->setFont(fontCondensed);

        // First ETA
        char etaText[8];
        if (dep.eta < 1)
            snprintf(etaText, sizeof(etaText), "<1'");
        else if (dep.eta >= 100)
            snprintf(etaText, sizeof(etaText), ">1h");
        else
            snprintf(etaText, sizeof(etaText), "%d'", dep.eta);

        if (dep.isDelayed && dep.delayMinutes > 0)
            display->setTextColor(COLOR_ORANGE);
        else if (dep.eta < 2)
            display->setTextColor(COLOR_RED);
        else if (dep.eta < 5)
            display->setTextColor(COLOR_YELLOW);
        else
            display->setTextColor(COLOR_WHITE);

        display->getTextBounds(etaText, 0, 0, &tx1, &ty1, &tw, &th);
        display->setCursor(114 - tw - tx1, y + 7);
        display->print(etaText);

        // Second ETA
        char eta2Text[8];
        if (dep.secondEta < 1)
            snprintf(eta2Text, sizeof(eta2Text), "<1'");
        else if (dep.secondEta >= 100)
            snprintf(eta2Text, sizeof(eta2Text), ">1h");
        else
            snprintf(eta2Text, sizeof(eta2Text), "%d'", dep.secondEta);

        if (dep.secondEta < 2)
            display->setTextColor(COLOR_RED);
        else if (dep.secondEta < 5)
            display->setTextColor(COLOR_YELLOW);
        else
            display->setTextColor(COLOR_WHITE);

        display->getTextBounds(eta2Text, 0, 0, &tx1, &ty1, &tw, &th);
        display->setCursor(127 - tw - tx1, y + 7);
        display->print(eta2Text);
    }
    else
    {
        // Single ETA mode: right-aligned to 127
        display->setFont(fontMedium);

        char etaText[8];
        if (dep.eta < 1)
            snprintf(etaText, sizeof(etaText), "<1'");
        else if (dep.eta >= 100)
            snprintf(etaText, sizeof(etaText), ">1h");
        else
            snprintf(etaText, sizeof(etaText), "%d'", dep.eta);

        if (dep.isDelayed && dep.delayMinutes > 0)
            display->setTextColor(COLOR_ORANGE);
        else if (dep.eta < 2)
            display->setTextColor(COLOR_RED);
        else if (dep.eta < 5)
            display->setTextColor(COLOR_YELLOW);
        else
            display->setTextColor(COLOR_WHITE);

        display->getTextBounds(etaText, 0, 0, &tx1, &ty1, &tw, &th);
        display->setCursor(127 - tw - tx1, y + 7);
        display->print(etaText);
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

            // Draw temperature right-aligned to degree symbol at X=103 (shifted +8px from X=93)
            // with its own color
            display->setTextColor(getTemperatureColor(weatherData->temperature));
            char tempStr[8];
            snprintf(tempStr, sizeof(tempStr), "%d\xB0", weatherData->temperature);

            // Calculate text width and right-align to degree anchor (X=103)
            int16_t x1, y1;
            uint16_t w, h;
            display->getTextBounds(tempStr, 0, 0, &x1, &y1, &w, &h);
            int tempX = 98 - w + x1; // Right-align to X=103, compensate for x1 offset

            display->setCursor(tempX, y + 7);
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
    sprintf(percentStr, "%d%%", percentage);

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
        sprintf(ipStr, "http://%s", WiFi.localIP().toString().c_str());
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

    // Calculate destX (same logic as drawDeparture)
    int destX = 22;
    if (dep.hasAC)
    {
        destX += 6;
    }

    // Calculate platform reserved space (same logic as drawDeparture)
    int platformReservedPx = 0;
    if (config && config->showPlatform && dep.platform[0] != '\0')
    {
        int platformLen = strlen(dep.platform);
        if (platformLen >= 3)
        {
            platformReservedPx = 15;
        }
        else if (platformLen == 2)
        {
            platformReservedPx = 11;
        }
        else
        {
            platformReservedPx = 9;
        }
    }

    // Calculate ETA position and available space
    bool dualEta = config && config->showMultipleTimes && dep.secondEta >= 0;
    int etaPosition;
    if (dualEta)
    {
        etaPosition = 103;
    }
    else
    {
        etaPosition = (dep.eta >= 10 || dep.eta < 1) ? 111 : 117;
    }
    int spaceCalcEta = dualEta ? 103 : ((platformReservedPx > 0) ? 111 : etaPosition);
    int availableSpace = spaceCalcEta - destX - platformReservedPx;

    // Determine font and maxChars (same logic as drawDeparture)
    int destLen = strlen(destConverted);
    const GFXfont *destFont;
    int maxChars;

    int mediumThreshold = platformReservedPx > 0 ? 12 : 14;
    mediumThreshold -= dep.hasAC ? 1 : 0;

    if (destLen <= mediumThreshold)
    {
        destFont = fontMedium;
        maxChars = availableSpace / 6;
    }
    else
    {
        destFont = fontCondensed;
        maxChars = availableSpace / 4 - 1; // -1 for padding
    }

    if (maxChars > 63)
        maxChars = 63;
    if (maxChars < 1)
        maxChars = 1;

    // Clear the destination area (from destX to just before platform/ETA)
    // Clear 8 pixels: 7 for the row (skip topmost) + 1 below baseline for descenders (y, g, p, q, j)
    // Font has no top overflows, so start at y+1 to preserve row above
    int clearWidth = spaceCalcEta - destX - platformReservedPx;
    display->fillRect(destX, y + 1, clearWidth, 8, COLOR_BLACK);

    // Apply scroll offset and draw
    display->setFont(destFont);
    display->setTextColor(COLOR_WHITE);
    display->setCursor(destX, y + 7);

    char destTrunc[64];
    int scrollOffset = scrollState[row].offset;
    if (scrollOffset > scrollState[row].maxOffset)
    {
        scrollOffset = scrollState[row].maxOffset;
    }
    strncpy(destTrunc, destConverted + scrollOffset, maxChars);
    destTrunc[maxChars] = '\0';
    display->print(destTrunc);
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
