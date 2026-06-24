#pragma once

// ============================================================================
// DisplayBridge — loop/API task -> display task hand-off (Phase 2)
// ============================================================================
// The only code that legitimately reads apiData-protected state and publishes it
// into displayRequest (under displayMutex), plus the Core-1 render task that
// consumes displayRequest. Co-locating both sides keeps the snapshot contract
// auditable: every apiDataMutex-protected field must be copied into a local while
// apiDataMutex is held, then written to displayRequest under displayMutex.

void signalDisplayUpdate();            // Snapshot state + notify the render task
void displayRenderTask(void* parameter); // Core-1 task: render displayRequest
