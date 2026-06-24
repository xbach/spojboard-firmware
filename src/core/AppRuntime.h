#pragma once

// ============================================================================
// AppRuntime — setup() / loop() glue helpers (Phase 4 of decomposition)
// ============================================================================
// Application-layer orchestration extracted from main.cpp's setup()/loop() so those
// stay short and readable. This layer may depend on every other module (AppState,
// AppCallbacks, DisplayBridge, TransitOrchestrator, utils), so the rest-mode scheduler
// etc. live here rather than being pushed down into the utils-layer RestMode (which must
// not depend upward). No concurrency change vs. the previous inline code.

// --- setup() helpers ---
void selectTransitAPI(); // Pick golemio/bvg/mqtt by config.city + wire the status callback

// --- loop() helpers (each owns its own timing statics; behaviour identical to before) ---
void pushWebServerState();      // Mirror current state into the web server status view
void serviceApModeDisplay();    // AP-mode periodic display refresh
void monitorWiFiConnection();   // STA-mode connect/disconnect transitions + reconnect
void checkScheduledRestMode();  // Enter/exit rest mode at :00 / :30
void serviceEtaRecalc();        // Recompute ETAs every 10s (skips demo/rest/ticker)
void serviceDisplayTicks();     // needsDisplayUpdate, scroll, infotext, 60s status log
