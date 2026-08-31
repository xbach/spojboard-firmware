#pragma once

#include "utils/RestPolicy.h" // RestDecision (rest-mode arbitration, TA-0254)

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
void checkScheduledRestMode();  // Enter/exit rest mode at :00 / :30 (edge-triggered, TA-0254)

// Applies a RestDecision's flags and side effects. Shared by the scheduler and the
// manual handler so waking can never restore the light on one path and forget to
// resume fetching on the other. See utils/RestPolicy.h for the arbitration rules.
void applyRestDecision(const RestDecision& d);

// Ask for one immediate schedule evaluation (boot, or a config save), instead of
// waiting for the next :00/:30.
void requestRestModeReevaluation();

// The schedule's current opinion, for the manual handler's override-label logic.
// READ-ONLY BY DESIGN -- there is no setter. A manual press must never move the
// schedule's opinion, or the next evaluation sees a false flip and the override
// evaporates (utils/RestPolicy.h).
int8_t currentScheduleOpinion();
void serviceEtaRecalc();        // Recompute ETAs every 10s (skips demo/rest/ticker)
void serviceDisplayTicks();     // needsDisplayUpdate, scroll, infotext, 60s status log
