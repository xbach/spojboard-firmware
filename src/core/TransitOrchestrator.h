#pragma once

// ============================================================================
// TransitOrchestrator — the departure data pipeline (Phase 3 of decomposition)
// ============================================================================
// Owns the Core-1 apiFetchTask (per-stop fetch -> persistent AccEntry accumulator
// -> ETA sort -> publishDepartureSnapshot under apiDataMutex) and the loop()-driven
// recalculateETAs(). All apiDataMutex *write* paths for departures[] now live in
// this one translation unit. Internal helpers (AccEntry/compareAccEntry/
// publishDepartureSnapshot/attachSecondETAs) and the retry constants are file-local
// in the .cpp.

void apiFetchTask(void* parameter); // Core-1 task: fetch departures/weather/ticker
void recalculateETAs();             // Called from loop(): refresh ETAs from cached timestamps
