#ifndef MQTTAPI_H
#define MQTTAPI_H

#include "TransitAPI.h"
#include "DepartureData.h"
#include "../config/AppConfig.h"
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

// ============================================================================
// MQTT Transit API Client
// ============================================================================

/**
 * MQTT-based Transit API Client
 *
 * Supports configurable JSON field mappings and both ETA and timestamp modes.
 * Connects to MQTT broker, publishes request, subscribes to response topic,
 * and parses JSON departures with user-defined field names.
 *
 * Key features:
 * - Optional username/password authentication
 * - ETA mode (pre-calculated minutes) vs Timestamp mode (unix timestamps)
 * - Configurable JSON field mappings for flexibility
 * - No minDepartureTime filtering (server-side filtering expected)
 */
class MqttAPI : public TransitAPI
{
  public:
    MqttAPI();
    virtual ~MqttAPI();

    /**
     * Set callback for API status updates
     * @param callback Function to call with status messages
     */
    virtual void setStatusCallback(APIStatusCallback callback) override;

    // MQTT aggregates all stops server-side, so it presents as a single "stop":
    // getStopCount() == 1 and fetchStop(0) returns the whole aggregated set.
    virtual int getStopCount(const Config& config) override;
    virtual StopResult fetchStop(const Config& config, int index) override;

  private:
    // Constants (must be declared before members that use them)
    static constexpr int JSON_BUFFER_SIZE = 8192; // 8KB for JSON parsing
    static constexpr int MQTT_BUFFER_SIZE = 8192; // 8KB for MQTT messages
    static constexpr int RESPONSE_TIMEOUT_MS = 10000; // 10 seconds
    static constexpr int CONNECT_TIMEOUT_MS = 5000; // 5 seconds
    static constexpr int MAX_TEMP_DEPARTURES = DEPS_PER_STOP * 12; // 144 departures

    WiFiClient wifiClient;
    PubSubClient* mqttClient;
    APIStatusCallback statusCallback;

    // Response handling
    volatile bool responseReceived;
    char* responseBuffer; // Heap buffer (avoids heap alloc in callback); allocated lazily, see ensureInitialized()
    unsigned int responseLength;
    unsigned long responseTimeout;

    // Whole-aggregate sort scratch (MQTT-only — Golemio/BVG write straight into StopResult).
    // Heap, allocated lazily so a non-MQTT device never pays the ~19KB. See ensureInitialized().
    Departure* tempDepartures;

    /**
     * Lazily allocate the heavy MQTT resources (PubSubClient + its buffer, responseBuffer,
     * tempDepartures) on first use — only a device actually running MQTT pays the ~27KB +
     * ~8KB. Idempotent; called at the top of every fetchStop. (TA-0190)
     * @return true if all resources are allocated, false on out-of-memory
     */
    bool ensureInitialized();

    /**
     * Connect to MQTT broker with optional authentication
     * @param config Configuration with broker address, port, username, password
     * @return true if connected successfully
     */
    bool connectToBroker(const Config& config);

    /**
     * MQTT message callback (static wrapper for PubSubClient)
     * Forwards to instance method via static pointer
     */
    static void messageCallback(char* topic, byte* payload, unsigned int length);

    /**
     * Instance method for message handling
     * Stores response payload and sets responseReceived flag
     */
    void handleMessage(char* topic, byte* payload, unsigned int length);

    /**
     * Wait for MQTT response with timeout
     * @return true if response received within timeout
     */
    bool waitForResponse();

    /**
     * Parse JSON response and populate departures array
     * @param config Configuration with field mappings and ETA mode
     * @param tempDepartures Array to populate
     * @param tempCount Output: number of departures parsed
     * @return true if parsing successful
     */
    bool parseResponse(const Config& config, Departure* tempDepartures, int& tempCount);

    /**
     * Extract string field value from JSON object using configured field name
     * @param obj JSON object
     * @param fieldName Field name from config
     * @param defaultValue Default if field missing
     * @return Field value or default
     */
    const char* getJsonField(JsonObject obj, const char* fieldName, const char* defaultValue);

    /**
     * Extract integer field value from JSON object using configured field name
     * @param obj JSON object
     * @param fieldName Field name from config
     * @param defaultValue Default if field missing
     * @return Field value or default
     */
    int getJsonFieldInt(JsonObject obj, const char* fieldName, int defaultValue);

    /**
     * Extract boolean field value from JSON object using configured field name
     * @param obj JSON object
     * @param fieldName Field name from config
     * @param defaultValue Default if field missing
     * @return Field value or default
     */
    bool getJsonFieldBool(JsonObject obj, const char* fieldName, bool defaultValue);

    // Static instance pointer for callback (required by PubSubClient)
    static MqttAPI* instanceForCallback;
};

#endif // MQTTAPI_H
