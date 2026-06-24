#ifndef TRANSITAPI_H
#define TRANSITAPI_H

#include "DepartureData.h"
#include "../config/AppConfig.h"

// ============================================================================
// Transit API Interface
// ============================================================================

/**
 * Abstract base class for transit API clients (Prague Golemio, Berlin BVG, etc.)
 * Defines common interface for fetching real-time departure information.
 */
class TransitAPI
{
  public:
    // Maximum length for concatenated infotexts (multiple joined with " /// ")
    static constexpr int MAX_INFOTEXT_LEN = 512;

    struct APIResult
    {
        Departure departures[MAX_DEPARTURES];
        int departureCount;
        char stopName[64];
        bool hasError;
        char errorMsg[64];
        char infoText[MAX_INFOTEXT_LEN]; // Concatenated service alerts/infotexts
    };

    // Result of fetching a SINGLE stop. Sized to MAX_DEPARTURES because MQTT
    // aggregates all stops server-side and returns its whole set as one "stop";
    // Golemio/BVG fill at most DEPS_PER_STOP. `hasError` lets the orchestrator
    // distinguish a failed fetch (keep stale rows) from a legitimately-empty one.
    struct StopResult
    {
        Departure departures[MAX_DEPARTURES];
        int departureCount;
        char stopName[64];
        bool hasError;
        char errorMsg[64];
        char infoText[MAX_INFOTEXT_LEN];
    };

    typedef void (*APIStatusCallback)(const char* message);

    virtual ~TransitAPI() = default;

    /**
     * Set callback for API status updates
     * @param callback Function to call with status messages
     */
    virtual void setStatusCallback(APIStatusCallback callback) = 0;

    /**
     * Fetch departures from transit API
     * @param config Configuration with API key, stop IDs, and filters
     * @return APIResult with departures, count, and error status
     */
    virtual APIResult fetchDepartures(const Config& config) = 0;

    /**
     * Number of stops this API iterates for a full fetch.
     * Prague/Berlin: count of configured stop IDs. MQTT: always 1 (server aggregates).
     */
    virtual int getStopCount(const Config& config) = 0;

    /**
     * Fetch a single stop by index in [0, getStopCount()). Lets the caller
     * (apiFetchTask) drive multi-stop orchestration and render incrementally.
     * @param config Configuration
     * @param index Zero-based stop index
     * @return StopResult for that stop (hasError distinguishes fail vs. empty)
     */
    virtual StopResult fetchStop(const Config& config, int index) = 0;
};

#endif // TRANSITAPI_H
