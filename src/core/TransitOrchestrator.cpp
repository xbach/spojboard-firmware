// ============================================================================
// TransitOrchestrator — definitions (Phase 3 of main.cpp decomposition)
// ============================================================================
// Moved verbatim from main.cpp: retry constants, attachSecondETAs, recalculateETAs,
// AccEntry/compareAccEntry, publishDepartureSnapshot, apiFetchTask. No behaviour
// change in this phase (the RACE 3 config guard lands in Phase 3b).

#include "core/TransitOrchestrator.h"

#include "core/AppState.h"
#include "core/DisplayBridge.h" // signalDisplayUpdate()
#include "utils/Logger.h"

// Defined in main.cpp (a config predicate shared by setup/loop/this task).
// The const Config& overload lets apiFetchTask check its config snapshot (RACE 3 fix).
bool isCityConfigured();
bool isCityConfigured(const Config& cfg);

// ============================================================================
// Failed-fetch retry cadence: after an error, retry sooner than the configured interval
// (but never wait longer than the normal interval). Prevents a single miss from blanking
// data for a full cycle, without busy-retrying every loop tick. Used only by apiFetchTask.
static const unsigned long DEPARTURES_RETRY_MS = 15000UL; // 15s
static const unsigned long WEATHER_RETRY_MS = 60000UL;    // 60s
// ============================================================================
// Second ETA Matching - Finds next departure for same line+destination
// ============================================================================
// showMultipleTimes passed in (not read from global config) so callers in the
// apiFetchTask context use their config snapshot — RACE 3 fix (TA-0225).
void attachSecondETAs(Departure* deps, int count, bool showMultipleTimes)
{
    for (int i = 0; i < count; i++)
    {
        deps[i].secondEta = -1;
        deps[i].secondDepartureTime = 0;
        if (!showMultipleTimes) continue;
        for (int j = i + 1; j < count; j++)
        {
            // Gate on sourceStopId: the secondary ETA must be the next departure of
            // this line+destination FROM THE SAME PHYSICAL STOP. Two stops in the same
            // direction can both serve the same line+destination, but a rider standing
            // at one stop can't board the bus leaving the other, so they must not be
            // conflated. (deps is sorted ascending by ETA, so the first match is the
            // soonest next departure.)
            //
            // The match must be a DIFFERENT trip, not a duplicate of this one. With
            // line+dest+stop already equal, a trip's identity is (departureTime,
            // delayMinutes) — departureTime being the real-time PREDICTED timestamp (see
            // GolemioAPI parse). Skip only when BOTH match: that's an API echoing the same
            // trip twice (or the same stop ID listed twice), which would otherwise show
            // secondEta == eta. Two genuinely different trains can share a predicted
            // instant — e.g. a delayed first train coinciding with the next — but their
            // schedules differ so their delayMinutes differ, keeping them valid seconds.
            bool sameTrip = (deps[j].departureTime == deps[i].departureTime &&
                             deps[j].delayMinutes == deps[i].delayMinutes);
            if (!sameTrip &&
                strcmp(deps[i].line, deps[j].line) == 0 &&
                strcmp(deps[i].destination, deps[j].destination) == 0 &&
                strcmp(deps[i].sourceStopId, deps[j].sourceStopId) == 0)
            {
                deps[i].secondEta = deps[j].eta;
                deps[i].secondDepartureTime = deps[j].departureTime;
                break;
            }
        }
    }
}

// ============================================================================
// ETA Recalculation - Updates ETAs from cached timestamps every 10s
// ============================================================================
void recalculateETAs()
{
    // Acquire mutex for thread-safe access to departure data
    if (!xSemaphoreTake(apiDataMutex, pdMS_TO_TICKS(100)))
    {
        logTimestamp();
        debugPrintln("ETA Recalc: Failed to acquire mutex, skipping");
        return;
    }

    // Recalculate ETAs from cached departureTime timestamps
    // Filter out stale departures (past their departure time)
    time_t now;
    time(&now);

    logTimestamp();
    debugPrint("ETA Recalc: ");
    debugPrint(departureCount);
    debugPrint(" deps -> ");

    int validCount = 0;
    for (int i = 0; i < departureCount; i++)
    {
        int diffSec = difftime(departures[i].departureTime, now);
        int eta = (diffSec > 0) ? (diffSec / 60) : 0;

        // Filter: Keep only FUTURE departures that meet minimum departure time threshold
        // (applies to all APIs including MQTT - filters during recalculation)
        // Note: diffSec > 0 allows eta=0 (1-59 seconds) when minDepartureTime=0
        if (diffSec > 0 && eta >= config.minDepartureTime)
        {
            // Copy departure if we're filtering out previous entries
            if (validCount != i)
            {
                departures[validCount] = departures[i];
            }
            departures[validCount].eta = eta;
            validCount++;
        }
    }

    debugPrint(validCount);
    debugPrint(" valid");
    if (validCount != departureCount)
    {
        debugPrint(" (filtered ");
        debugPrint(departureCount - validCount);
        debugPrint(")");
    }
    debugPrintln("");

    departureCount = validCount;

    // Resort departures by ETA after recalculation
    if (departureCount > 1)
    {
        logTimestamp();
        debugPrintln("ETA Recalc: Resorting departures by ETA");
        qsort(departures, departureCount, sizeof(Departure), compareDepartures);

        // Log final order (first 3)
        for (int i = 0; i < departureCount && i < 3; i++)
        {
            logTimestamp();
            char sortMsg[96];
            snprintf(sortMsg,
                     sizeof(sortMsg),
                     "  After sort [%d]: Line %s, ETA=%d min",
                     i,
                     departures[i].line,
                     departures[i].eta);
            debugPrintln(sortMsg);
        }
    }

    // Recompute second ETAs after re-sort (matching pairs may have changed).
    // recalculateETAs() runs in loop context (same task as config's writer), so reading
    // the global config.showMultipleTimes here is race-free.
    attachSecondETAs(departures, departureCount, config.showMultipleTimes);

    // Release mutex
    xSemaphoreGive(apiDataMutex);

    // Reset scroll state since departures may have changed positions
    displayManager.resetScroll();

    logTimestamp();
    debugPrintln("ETA Recalc: Complete, display update triggered");
    signalDisplayUpdate();
}
// Per-stop accumulator entry. The stop index that produced a departure MUST travel
// WITH the departure through qsort — a parallel `int[]` tag array desyncs the instant
// qsort permutes only the Departure array, which then mis-targets the per-stop eviction
// and produces duplicate trips. Binding the tag into the sorted record makes them
// physically inseparable. (RAM-neutral: the int tag existed before as a parallel array.)
struct AccEntry
{
    Departure dep;
    int stopIndex;
};

// qsort comparator over AccEntry — delegates to compareDepartures on the embedded
// departure so the stopIndex tag is co-permuted with its departure.
static int compareAccEntry(const void* a, const void* b)
{
    return compareDepartures(&((const AccEntry*)a)->dep, &((const AccEntry*)b)->dep);
}

// ============================================================================
// Departure publish helper (Stage B: shared by the per-stop orchestration)
// ============================================================================
// Builds the displayed snapshot from the per-stop accumulator and copies it into the
// shared departures[] under apiDataMutex. The accumulator MUST already be sorted by ETA;
// this filters stale / below-minDepartureTime rows, caps to MAX_DEPARTURES, attaches
// second ETAs, and signals the display. Mutex is held only for the fast copy
// (build-then-swap) so the display task never sees a half-written array.
// Called only from apiFetchTask (single task) — the static snapshot buffer is safe.
// minDepartureTime / showMultipleTimes passed in (from apiFetchTask's config snapshot)
// rather than read from the global config — RACE 3 fix (TA-0225).
static void publishDepartureSnapshot(const AccEntry* acc, int accCount, const char* snapStopName,
                                     const char* snapInfoText, bool hasError, const char* errorMsg,
                                     int minDepartureTime, bool showMultipleTimes)
{
    static Departure snapshot[MAX_DEPARTURES];
    int snapCount = 0;
    time_t now;
    time(&now);

    for (int i = 0; i < accCount && snapCount < MAX_DEPARTURES; i++)
    {
        int diffSec = difftime(acc[i].dep.departureTime, now);
        int eta = (diffSec > 0) ? (diffSec / 60) : 0;
        // Keep only future departures meeting the minimum threshold (eta=0 = <1min).
        if (diffSec > 0 && eta >= minDepartureTime)
        {
            snapshot[snapCount] = acc[i].dep;
            snapshot[snapCount].eta = eta;
            snapCount++;
        }
    }

    // Second ETAs are computed on the filtered+sorted snapshot, gated by sourceStopId.
    attachSecondETAs(snapshot, snapCount, showMultipleTimes);

    if (xSemaphoreTake(apiDataMutex, pdMS_TO_TICKS(100)))
    {
        departureCount = snapCount;
        for (int i = 0; i < snapCount; i++)
            departures[i] = snapshot[i];
        strlcpy(stopName, snapStopName, sizeof(stopName));
        strlcpy(infoText, snapInfoText, sizeof(infoText));
        apiError = hasError;
        if (hasError)
            strlcpy(apiErrorMsg, errorMsg, sizeof(apiErrorMsg));
        awaitingDepartures = false;
        xSemaphoreGive(apiDataMutex);

        signalDisplayUpdate();
    }
    else
    {
        logTimestamp();
        debugPrintln("APIFetchTask: Failed to acquire mutex for departures publish");
    }
}

// ============================================================================
// API Fetch Task - Runs on Core 1 (handles blocking HTTP calls)
// ============================================================================
void apiFetchTask(void* parameter)
{
    logTimestamp();
    debugPrintln("APIFetchTask: Started on Core 1");

    for (;;)
    {
        // RACE 3 fix (TA-0225): snapshot the whole config once per iteration under
        // configMutex, then read only from `cfg` below (incl. getStopCount/fetchStop and
        // isCityConfigured(cfg)). config's sole writer is onConfigSave in loop context;
        // this task is the only cross-task reader, so guarding these two edges removes the
        // torn multi-byte read. Static (.bss) — the ~2.5KB Config never lands on this
        // task's 10KB stack. Mutex held only for the memcpy, never across an HTTP fetch.
        static Config cfg;
        if (xSemaphoreTake(configMutex, pdMS_TO_TICKS(100)))
        {
            cfg = config;
            xSemaphoreGive(configMutex);
        }

        unsigned long now = millis();
        bool shouldFetchDepartures = false;
        bool shouldFetchWeather = false;
        bool shouldFetchTicker = false;

        // Check if immediate fetch is requested (from config save, refresh button, etc.)
        if (apiFetchRequest.fetchDeparturesNow)
        {
            apiFetchRequest.fetchDeparturesNow = false;
            shouldFetchDepartures = true;
            logTimestamp();
            debugPrintln("APIFetchTask: Immediate departures fetch requested");
        }

        if (apiFetchRequest.fetchWeatherNow)
        {
            apiFetchRequest.fetchWeatherNow = false;
            shouldFetchWeather = true;
            logTimestamp();
            debugPrintln("APIFetchTask: Immediate weather fetch requested");
        }

        if (apiFetchRequest.fetchTickerNow)
        {
            apiFetchRequest.fetchTickerNow = false;
            shouldFetchTicker = true;
            logTimestamp();
            debugPrintln("APIFetchTask: Immediate ticker fetch requested");
        }

        // Check periodic fetch intervals (only if not in demo/rest mode and WiFi connected)
        // CRITICAL: Also require timezone to be initialized to prevent timestamp parsing with wrong timezone
        if (!demoModeActive && !restModeActive && wifiManager.isConnected() && apiFetchRequest.timezoneInitialized)
        {
            // Departures fetch interval (blocked during ticker mode)
            if (!tickerModeActive && isCityConfigured(cfg))
            {
                unsigned long departuresInterval = (unsigned long)cfg.refreshInterval * 1000;
                if (apiFetchRequest.departuresRetryPending && DEPARTURES_RETRY_MS < departuresInterval)
                    departuresInterval = DEPARTURES_RETRY_MS;
                if (now - apiFetchRequest.lastDeparturesFetch >= departuresInterval ||
                    apiFetchRequest.lastDeparturesFetch == 0)
                {
                    shouldFetchDepartures = true;
                }
            }

            // Weather fetch interval (allowed during ticker - status bar visible)
            if (cfg.weatherEnabled && cfg.weatherLatitude != 0.0 && cfg.weatherLongitude != 0.0)
            {
                unsigned long weatherInterval = (unsigned long)cfg.weatherRefreshInterval * 60000;
                if (apiFetchRequest.weatherRetryPending && WEATHER_RETRY_MS < weatherInterval)
                    weatherInterval = WEATHER_RETRY_MS;
                if (now - apiFetchRequest.lastWeatherFetch >= weatherInterval ||
                    apiFetchRequest.lastWeatherFetch == 0)
                {
                    shouldFetchWeather = true;
                }
            }

            // Ticker fetch interval (only when ticker active)
            if (tickerModeActive && cfg.tickerApiKey[0] != '\0')
            {
                unsigned long tickerInterval = (unsigned long)cfg.tickerRefreshInterval * 1000;
                if (now - apiFetchRequest.lastTickerFetch >= tickerInterval ||
                    apiFetchRequest.lastTickerFetch == 0)
                {
                    shouldFetchTicker = true;
                }
            }
        }

        // Fetch departures — per-stop orchestration (hoisted from the API clients in
        // Stage B). Each configured stop is fetched individually, merged into a persistent
        // accumulator, and (during the initial fill) published incrementally so the board
        // fills progressively instead of staying blank until the slowest stop returns.
        if (shouldFetchDepartures)
        {
            logTimestamp();
            debugPrint("APIFetchTask: Fetching departures per-stop (minDepTime=");
            debugPrint(cfg.minDepartureTime);
            debugPrintln(")");

            // Log device time for time-sync debugging.
            struct tm timeinfo;
            if (getLocalTime(&timeinfo))
            {
                char deviceTimeStr[32];
                strftime(deviceTimeStr, sizeof(deviceTimeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
                logTimestamp();
                debugPrint("APIFetchTask: Device time = ");
                debugPrintln(deviceTimeStr);
            }
            else
            {
                logTimestamp();
                debugPrintln("⚠️ APIFetchTask: Device time not synced!");
            }

            // Persistent accumulator (survives across cycles for keep-stale-per-stop).
            // acc[i].stopIndex records which stop produced acc[i].dep, so a stop's old rows
            // are evicted on its next SUCCESSFUL fetch without touching other stops — and
            // without relying on sourceStopId (MQTT sets it freely). The tag is bound INTO
            // AccEntry (not a parallel array) so it co-permutes with its departure through
            // the ETA qsort; a parallel array would desync and mis-target the eviction.
            // Static: lives in .bss, never on the task stack, never pruned mid-cycle.
            static AccEntry acc[DEPS_PER_STOP * 12];
            static int accCount = 0;
            const int ACC_CAPACITY = DEPS_PER_STOP * 12;

            int stopCount = transitAPI->getStopCount(cfg);

            // Drop rows from stops that no longer exist (stop list shrank without reboot).
            {
                int w = 0;
                for (int i = 0; i < accCount; i++)
                {
                    if (acc[i].stopIndex < stopCount)
                    {
                        if (w != i)
                            acc[w] = acc[i];
                        w++;
                    }
                }
                accCount = w;
            }

            // Publish progressively only while the board is empty (initial fill); in steady
            // state replace silently and publish once at the end to avoid render churn.
            bool incrementalPublish = (departureCount == 0) || awaitingDepartures;

            bool anyError = false;
            char cycleStopName[64] = "";
            char cycleInfoText[TransitAPI::MAX_INFOTEXT_LEN] = "";

            for (int s = 0; s < stopCount; s++)
            {
                TransitAPI::StopResult sr = transitAPI->fetchStop(cfg, s);

                if (sr.hasError)
                {
                    // Keep this stop's previous rows — a failed fetch must not blank it.
                    anyError = true;
                    logTimestamp();
                    debugPrint("APIFetchTask: stop ");
                    debugPrint(s);
                    debugPrint(" error (keeping previous) - ");
                    debugPrintln(sr.errorMsg);
                }
                else
                {
                    // Evict this stop's old rows, then insert its fresh ones. The stopIndex
                    // tag travels inside AccEntry, so it stays correct across the ETA sort.
                    int w = 0;
                    for (int i = 0; i < accCount; i++)
                    {
                        if (acc[i].stopIndex != s)
                        {
                            if (w != i)
                                acc[w] = acc[i];
                            w++;
                        }
                    }
                    accCount = w;
                    for (int i = 0; i < sr.departureCount && accCount < ACC_CAPACITY; i++)
                    {
                        acc[accCount].dep = sr.departures[i];
                        acc[accCount].stopIndex = s;
                        accCount++;
                    }
                }

                // First non-empty stop name wins (matches prior first-stop behavior);
                // concatenate infotexts across stops as the old shared buffer did.
                if (cycleStopName[0] == '\0' && sr.stopName[0] != '\0')
                    strlcpy(cycleStopName, sr.stopName, sizeof(cycleStopName));
                if (sr.infoText[0] != '\0')
                {
                    if (cycleInfoText[0] != '\0')
                        strlcat(cycleInfoText, " /// ", sizeof(cycleInfoText));
                    strlcat(cycleInfoText, sr.infoText, sizeof(cycleInfoText));
                }

                // Sort accumulator by ETA so the published cap keeps the soonest. The
                // comparator co-permutes each entry's stopIndex tag with its departure.
                if (accCount > 1)
                    qsort(acc, accCount, sizeof(AccEntry), compareAccEntry);

                // Progressive publish during initial fill (skip the last stop — the
                // post-loop publish covers it).
                if (incrementalPublish && s < stopCount - 1)
                    publishDepartureSnapshot(acc, accCount, cycleStopName, cycleInfoText, false, "",
                                             cfg.minDepartureTime, cfg.showMultipleTimes);

                if (s < stopCount - 1)
                    delay(1000); // inter-stop rate limiting (hoisted from the clients)
            }

            // Final publish of the complete cycle. Error flag matches the old semantics:
            // set only when there's nothing to show (keep-stale means a cached prior cycle
            // keeps accCount > 0 even if every stop failed this round).
            bool cycleError = (accCount == 0);
            publishDepartureSnapshot(acc, accCount, cycleStopName, cycleInfoText, cycleError,
                                     cycleError ? "No departures" : "",
                                     cfg.minDepartureTime, cfg.showMultipleTimes);

            logTimestamp();
            debugPrintln("APIFetchTask: Departures fetch complete");

            // One-shot heap watermark after the first fetch (HTTPS handshake is the peak).
            static bool loggedPostFetch = false;
            if (!loggedPostFetch)
            {
                loggedPostFetch = true;
                logMemory("post_fetch");
            }

            // Schedule next from COMPLETION time (not stale loop-top now); retry sooner if
            // any stop failed this cycle.
            apiFetchRequest.lastDeparturesFetch = millis();
            apiFetchRequest.departuresRetryPending = anyError;
        }

        // Fetch weather (blocking HTTP call)
        if (shouldFetchWeather)
        {
            logTimestamp();
            debugPrintln("APIFetchTask: Fetching weather (blocking)...");

            // Call weather API (blocking operation)
            WeatherData newWeatherData = weatherAPI.fetchWeather(cfg.weatherLatitude, cfg.weatherLongitude);

            // Update global weather state with mutex protection
            if (xSemaphoreTake(apiDataMutex, pdMS_TO_TICKS(100)))
            {
                // Keep the previous weatherData on error — a stale temperature beats a
                // blank status bar (mirrors the ticker pattern). Only overwrite on success.
                if (!newWeatherData.hasError)
                {
                    weatherData = newWeatherData;
                }
                xSemaphoreGive(apiDataMutex);

                // Signal display update
                signalDisplayUpdate();

                if (newWeatherData.hasError)
                {
                    logTimestamp();
                    debugPrint("APIFetchTask: Weather error (keeping previous) - ");
                    debugPrintln(newWeatherData.errorMsg);
                }
                else
                {
                    logTimestamp();
                    char msg[64];
                    snprintf(msg, sizeof(msg), "APIFetchTask: Weather updated: %d°C", newWeatherData.temperature);
                    debugPrintln(msg);
                }
            }
            else
            {
                logTimestamp();
                debugPrintln("APIFetchTask: Failed to acquire mutex for weather update");
            }

            // Schedule next fetch from completion; retry sooner on error so a single miss
            // doesn't leave the temperature stale for a whole refresh interval.
            apiFetchRequest.lastWeatherFetch = millis();
            apiFetchRequest.weatherRetryPending = newWeatherData.hasError;
        }

        // Fetch ticker data (blocking HTTP call)
        if (shouldFetchTicker)
        {
            apiFetchRequest.lastTickerFetch = now;

            logTimestamp();
            debugPrintln("APIFetchTask: Fetching ticker (blocking)...");

            // Call Twelve Data API (blocking operation)
            TickerData newTickerData = tickerAPI.fetchTicker(
                cfg.tickerSymbol, cfg.tickerInterval, cfg.tickerApiKey);

            // Update global ticker state with mutex protection
            if (xSemaphoreTake(apiDataMutex, pdMS_TO_TICKS(100)))
            {
                // Keep last valid data on error
                if (newTickerData.valid || !tickerData.valid)
                {
                    tickerData = newTickerData;
                }
                else if (newTickerData.hasError)
                {
                    tickerData.hasError = true; // Flag error but keep old candles
                }
                xSemaphoreGive(apiDataMutex);

                // Signal display update
                signalDisplayUpdate();

                logTimestamp();
                if (newTickerData.valid)
                {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "APIFetchTask: Ticker updated: %d candles", newTickerData.candleCount);
                    debugPrintln(msg);
                }
                else
                {
                    debugPrintln("APIFetchTask: Ticker fetch failed");
                }
            }
            else
            {
                logTimestamp();
                debugPrintln("APIFetchTask: Failed to acquire mutex for ticker update");
            }
        }

        // Sleep for 100ms between checks (prevents busy-waiting, allows web server to run)
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
