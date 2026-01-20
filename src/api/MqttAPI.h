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

    /**
     * Fetch departures via MQTT request/response
     * @param config Configuration with MQTT broker, topics, and field mappings
     * @return APIResult with departures, count, and error status
     */
    virtual APIResult fetchDepartures(const Config& config) override;

  private:
    WiFiClient wifiClient;
    PubSubClient* mqttClient;
    APIStatusCallback statusCallback;

    // Response handling
    bool responseReceived;
    String responsePayload;
    unsigned long responseTimeout;

    // Constants
    static constexpr int JSON_BUFFER_SIZE = 8192; // 8KB for JSON parsing
    static constexpr int MQTT_BUFFER_SIZE = 8192; // 8KB for MQTT messages
    static constexpr int RESPONSE_TIMEOUT_MS = 10000; // 10 seconds
    static constexpr int CONNECT_TIMEOUT_MS = 5000; // 5 seconds
    static constexpr int MAX_TEMP_DEPARTURES = MAX_DEPARTURES * 12; // 144 departures

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
