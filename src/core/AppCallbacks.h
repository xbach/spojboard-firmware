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
void onConfigSave(const Config& newConfig, bool wifiChanged, const char* tab);
void onRefresh();
void onReboot();
void onDemoStart(const Departure* demoDepartures, int demoCount);
void onDemoStop();
void onRestMode(bool enabled);
void onTickerStart();
void onTickerStop();
void onTickerMode(bool enabled);
