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
    struct APIResult
    {
        Departure departures[MAX_DEPARTURES];
        int departureCount;
        char stopName[64];
        bool hasError;
        char errorMsg[64];
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
};

#endif // TRANSITAPI_H
