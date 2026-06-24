#ifndef BVGAPI_H
#define BVGAPI_H

#include "TransitAPI.h"
#include "DepartureData.h"
#include "../config/AppConfig.h"
#include <ArduinoJson.h>

// ============================================================================
// BVG API Client
// ============================================================================

/**
 * Client for Berlin's BVG API (v6.bvg.transport.rest)
 * Fetches real-time departure information for public transit stops.
 */
class BvgAPI : public TransitAPI
{
  public:
    BvgAPI();

    /**
     * Set callback for API status updates
     * @param callback Function to call with status messages
     */
    virtual void setStatusCallback(APIStatusCallback callback) override;

    virtual int getStopCount(const Config& config) override;
    virtual StopResult fetchStop(const Config& config, int index) override;

  private:
    APIStatusCallback statusCallback;
    // BVG responses are verbose (>2.7KB/departure at busy hubs). We parse with an
    // ArduinoJson Filter and stream directly from the socket, so the document only
    // holds the few fields we use — keeping RAM low on 4-panel builds.
    static constexpr int PARSE_DOC_SIZE = 8192;    // filtered document (only fields we read)
    static constexpr int JSON_BUFFER_SIZE = 32768; // read cap; must exceed the real response size
    static constexpr int HTTP_TIMEOUT_MS = 15000;
    // Departures requested per stop. Independent of DEPS_PER_STOP (12): a busy hub's BVG
    // payload is >2.7KB/dep, so 12 overflows the 32KB read cap → truncated → IncompleteInput.
    // 10 is the proven-safe ceiling that stays under the cap. Golemio's compact payload has
    // no such limit and requests the full DEPS_PER_STOP.
    static constexpr int BVG_MAX_RESULTS = 10;

    /**
     * Query a single stop and add results to temp array
     * @param stopId Stop ID to query
     * @param config Configuration
     * @param tempDepartures Temporary array to fill
     * @param tempCount Current count in temp array
     * @param stopName Output: stop name (from first stop only)
     * @param isFirstStop Whether this is the first stop being queried
     * @return true if query succeeded
     */
    bool querySingleStop(const char* stopId,
                         const Config& config,
                         Departure* tempDepartures,
                         int& tempCount,
                         char* stopName,
                         bool& isFirstStop);

    /**
     * Parse departure JSON object and add to temp array
     * @param depJson JSON object for single departure
     * @param config Configuration (for debug flag)
     * @param tempDepartures Array to add to
     * @param tempCount Current count (will be incremented)
     */
    void parseDepartureObject(JsonObject depJson, const Config& config, Departure* tempDepartures, int& tempCount, const char* stopId);
};

#endif // BVGAPI_H
