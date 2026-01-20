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

    /**
     * Fetch departures from BVG API
     * @param config Configuration with stop IDs and filters
     * @return APIResult with departures, count, and error status
     */
    virtual APIResult fetchDepartures(const Config& config) override;

  private:
    APIStatusCallback statusCallback;
    static constexpr int MAX_TEMP_DEPARTURES = MAX_DEPARTURES * 12; // Buffer for up to 12 stops at full capacity
    static constexpr int JSON_BUFFER_SIZE = 24576; // 24KB - BVG API returns verbose responses (~1.7KB per departure)
    static constexpr int HTTP_TIMEOUT_MS = 10000;

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
    void parseDepartureObject(JsonObject depJson, const Config& config, Departure* tempDepartures, int& tempCount);
};

#endif // BVGAPI_H
