#pragma once

// ============================================================================
// AppCallbacks — web-server / API callbacks (Phase 1 of main.cpp decomposition)
// ============================================================================
// Pure glue that the network layer calls upward into the app: set a cross-task
// flag, persist config, and/or signal a display update. Wired in setup() via
// webServer.setCallbacks(...) and transitAPI->setStatusCallback(...).
//
// All shared state these touch is declared in core/AppState.h.

#include "config/AppConfig.h"  // Config
#include "api/DepartureData.h" // Departure

void onAPIStatus(const char* message);
// `needsRestart` is NOT "the WiFi changed" -- it is the union of every change that
// only takes effect at boot (WiFi, city, panel arrangement, panel wiring). It was
// called wifiChanged, and a caller adding a new boot-only setting then had no name
// telling them to include it; the wiring profile was omitted for exactly that reason.
void onConfigSave(const Config& newConfig, bool needsRestart, const char* tab);
void onRefresh();
void onReboot();
void onDemoStart(const Departure* demoDepartures, int demoCount);
void onDemoStop();

/**
 * Draw the panel colour test (TA-0302). Called from the web layer via callback
 * so the display lock stays in this layer -- ConfigWebServer must not take
 * displayHwMutex itself, because lower layers never depend on higher ones.
 */
void onHwTestPattern();
void onRestMode(bool enabled);
void onTickerStart();
void onTickerStop();
void onTickerMode(bool enabled);
