#include "MqttAPI.h"
#include "../utils/Logger.h"
#include <Arduino.h>
#include <WiFi.h>

// ============================================================================
// Static Members
// ============================================================================

MqttAPI* MqttAPI::instanceForCallback = nullptr;

// ============================================================================
// Constructor / Destructor
// ============================================================================

MqttAPI::MqttAPI() : statusCallback(nullptr), responseReceived(false), responseTimeout(0)
{
    mqttClient = new PubSubClient(wifiClient);
    mqttClient->setBufferSize(MQTT_BUFFER_SIZE); // CRITICAL: Must be called before connect
    mqttClient->setCallback(messageCallback);

    instanceForCallback = this; // Set static pointer for callback
}

MqttAPI::~MqttAPI()
{
    if (mqttClient)
    {
        if (mqttClient->connected())
        {
            mqttClient->disconnect();
        }
        delete mqttClient;
        mqttClient = nullptr;
    }

    if (instanceForCallback == this)
    {
        instanceForCallback = nullptr;
    }
}

// ============================================================================
// TransitAPI Interface Implementation
// ============================================================================

void MqttAPI::setStatusCallback(APIStatusCallback callback)
{
    statusCallback = callback;
}

TransitAPI::APIResult MqttAPI::fetchDepartures(const Config& config)
{
    APIResult result;
    result.departureCount = 0;
    result.hasError = false;
    strlcpy(result.stopName, "MQTT", sizeof(result.stopName));

    logTimestamp();
    debugPrintln("MQTT: Fetching departures...");

    // Validate configuration
    if (strlen(config.mqttBroker) == 0)
    {
        logTimestamp();
        debugPrintln("MQTT: Error - No broker configured");
        result.hasError = true;
        strlcpy(result.errorMsg, "MQTT: Not configured", sizeof(result.errorMsg));
        if (statusCallback)
            statusCallback("MQTT: Not configured");
        return result;
    }

    if (strlen(config.mqttRequestTopic) == 0 || strlen(config.mqttResponseTopic) == 0)
    {
        logTimestamp();
        debugPrintln("MQTT: Error - Topics not configured");
        result.hasError = true;
        strlcpy(result.errorMsg, "MQTT: Topics missing", sizeof(result.errorMsg));
        if (statusCallback)
            statusCallback("MQTT: Topics missing");
        return result;
    }

    if (strlen(config.mqttFieldLine) == 0 || strlen(config.mqttFieldDestination) == 0)
    {
        logTimestamp();
        debugPrintln("MQTT: Error - Field mappings incomplete");
        result.hasError = true;
        strlcpy(result.errorMsg, "MQTT: Fields incomplete", sizeof(result.errorMsg));
        if (statusCallback)
            statusCallback("MQTT: Fields incomplete");
        return result;
    }

    // Connect to broker
    if (!connectToBroker(config))
    {
        logTimestamp();
        debugPrintln("MQTT: Connection failed");
        result.hasError = true;
        strlcpy(result.errorMsg, "MQTT: Connection failed", sizeof(result.errorMsg));
        if (statusCallback)
            statusCallback("MQTT: Connection failed");
        return result;
    }

    // Reset response state
    responseReceived = false;
    responsePayload = "";

    // Publish request message
    logTimestamp();
    debugPrint("MQTT: Publishing to ");
    debugPrintln(config.mqttRequestTopic);

    if (!mqttClient->publish(config.mqttRequestTopic, "request"))
    {
        logTimestamp();
        debugPrintln("MQTT: Publish failed");
        result.hasError = true;
        strlcpy(result.errorMsg, "MQTT: Publish failed", sizeof(result.errorMsg));
        if (statusCallback)
            statusCallback("MQTT: Publish failed");
        mqttClient->disconnect();
        return result;
    }

    // Wait for response
    if (!waitForResponse())
    {
        logTimestamp();
        debugPrintln("MQTT: Response timeout");
        result.hasError = true;
        strlcpy(result.errorMsg, "MQTT: Response timeout", sizeof(result.errorMsg));
        if (statusCallback)
            statusCallback("MQTT: Response timeout");
        mqttClient->disconnect();
        return result;
    }

    // Parse response
    static Departure tempDepartures[MAX_TEMP_DEPARTURES];
    int tempCount = 0;

    if (!parseResponse(config, tempDepartures, tempCount))
    {
        logTimestamp();
        debugPrintln("MQTT: Parse error");
        result.hasError = true;
        strlcpy(result.errorMsg, "MQTT: Parse error", sizeof(result.errorMsg));
        if (statusCallback)
            statusCallback("MQTT: Parse error");
        mqttClient->disconnect();
        return result;
    }

    // Disconnect (reconnect on next call - simpler and more robust)
    mqttClient->disconnect();

    // Sort by ETA
    if (tempCount > 0)
    {
        qsort(tempDepartures, tempCount, sizeof(Departure), compareDepartures);
    }

    // Copy results to API result (no minDepartureTime filtering for MQTT!)
    result.departureCount = (tempCount > MAX_DEPARTURES) ? MAX_DEPARTURES : tempCount;
    for (int i = 0; i < result.departureCount; i++)
    {
        result.departures[i] = tempDepartures[i];
    }

    logTimestamp();
    debugPrint("MQTT: Received ");
    debugPrint(result.departureCount);
    debugPrintln(" departures");

    return result;
}

// ============================================================================
// Connection Management
// ============================================================================

bool MqttAPI::connectToBroker(const Config& config)
{
    // Already connected?
    if (mqttClient->connected())
    {
        return true;
    }

    // Generate unique client ID from MAC address
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char clientId[32];
    snprintf(clientId, sizeof(clientId), "spojboard-%02X%02X%02X", mac[3], mac[4], mac[5]);

    logTimestamp();
    debugPrint("MQTT: Connecting to ");
    debugPrint(config.mqttBroker);
    debugPrint(":");
    debugPrint(config.mqttPort);
    debugPrintln("");

    // Set server
    mqttClient->setServer(config.mqttBroker, config.mqttPort);

    // Connect with or without authentication
    bool connected = false;
    unsigned long connectStart = millis();

    while (!connected && (millis() - connectStart < CONNECT_TIMEOUT_MS))
    {
        if (strlen(config.mqttUsername) > 0)
        {
            // Connect with authentication
            connected = mqttClient->connect(clientId, config.mqttUsername, config.mqttPassword);
        }
        else
        {
            // Connect without authentication
            connected = mqttClient->connect(clientId);
        }

        if (!connected)
        {
            delay(500);
        }
    }

    if (!connected)
    {
        logTimestamp();
        debugPrint("MQTT: Connect failed, state: ");
        debugPrint(mqttClient->state());
        debugPrintln("");
        return false;
    }

    // Subscribe to response topic
    if (!mqttClient->subscribe(config.mqttResponseTopic))
    {
        logTimestamp();
        debugPrintln("MQTT: Subscribe failed");
        mqttClient->disconnect();
        return false;
    }

    logTimestamp();
    debugPrint("MQTT: Subscribed to ");
    debugPrintln(config.mqttResponseTopic);

    // Small delay to ensure subscription is processed
    delay(100);

    return true;
}

// ============================================================================
// MQTT Callback Handling
// ============================================================================

void MqttAPI::messageCallback(char* topic, byte* payload, unsigned int length)
{
    if (instanceForCallback != nullptr)
    {
        instanceForCallback->handleMessage(topic, payload, length);
    }
}

void MqttAPI::handleMessage(char* topic, byte* payload, unsigned int length)
{
    logTimestamp();
    debugPrint("MQTT: Message received (");
    debugPrint(length);
    debugPrintln(" bytes)");

    responsePayload = String((char*)payload, length);
    responseReceived = true;
}

bool MqttAPI::waitForResponse()
{
    responseTimeout = millis() + RESPONSE_TIMEOUT_MS;

    while (!responseReceived && millis() < responseTimeout)
    {
        mqttClient->loop(); // Process incoming messages
        delay(10);
    }

    return responseReceived;
}

// ============================================================================
// JSON Parsing
// ============================================================================

bool MqttAPI::parseResponse(const Config& config, Departure* tempDepartures, int& tempCount)
{
    tempCount = 0;

    // Parse JSON
    DynamicJsonDocument doc(JSON_BUFFER_SIZE);
    DeserializationError error = deserializeJson(doc, responsePayload);

    if (error)
    {
        logTimestamp();
        debugPrint("MQTT: JSON parse error: ");
        debugPrintln(error.c_str());
        return false;
    }

    // Check for departures array
    if (!doc.containsKey("departures"))
    {
        logTimestamp();
        debugPrintln("MQTT: No 'departures' field in JSON");
        return false;
    }

    JsonArray departures = doc["departures"].as<JsonArray>();
    if (departures.size() == 0)
    {
        logTimestamp();
        debugPrintln("MQTT: Empty departures array");
        return false;
    }

    logTimestamp();
    debugPrint("MQTT: Parsing ");
    debugPrint(departures.size());
    debugPrintln(" departures");

    // Get current time for ETA calculations
    time_t now = time(nullptr);

    // Parse each departure
    for (JsonObject depObj : departures)
    {
        if (tempCount >= MAX_TEMP_DEPARTURES)
        {
            logTimestamp();
            debugPrintln("MQTT: Max departures reached");
            break;
        }

        Departure dep;
        memset(&dep, 0, sizeof(dep));

        // Extract line number (required)
        const char* line = getJsonField(depObj, config.mqttFieldLine, "");
        if (strlen(line) == 0)
        {
            continue; // Skip if no line number
        }
        strlcpy(dep.line, line, sizeof(dep.line));
        stripSpaces(dep.line);
        stripBrackets(dep.line);

        // Extract destination (required)
        const char* dest = getJsonField(depObj, config.mqttFieldDestination, "");
        if (strlen(dest) == 0)
        {
            continue; // Skip if no destination
        }
        strlcpy(dep.destination, dest, sizeof(dep.destination));

        // Apply destination shortening (like GolemioAPI)
        shortenDestination(dep.destination);

        // ETA handling - depends on configured mode
        if (config.mqttUseEtaMode)
        {
            // ETA Mode: Read ETA field directly (minutes)
            dep.eta = getJsonFieldInt(depObj, config.mqttFieldEta, -1);
            if (dep.eta < 0)
            {
                continue; // Skip if no ETA
            }

            // Synthesize departureTime for recalculation compatibility
            dep.departureTime = now + (dep.eta * 60);
        }
        else
        {
            // Timestamp Mode: Read timestamp field (unix seconds)
            dep.departureTime = getJsonFieldInt(depObj, config.mqttFieldTimestamp, 0);
            if (dep.departureTime == 0)
            {
                continue; // Skip if no timestamp
            }

            // Calculate ETA from timestamp
            dep.eta = calculateETA(dep.departureTime);
            if (dep.eta < 0)
            {
                continue; // Skip if already departed
            }
        }

        // Extract optional platform field
        const char* platform = getJsonField(depObj, config.mqttFieldPlatform, "");
        strlcpy(dep.platform, platform, sizeof(dep.platform));

        // Extract optional AC field
        dep.hasAC = getJsonFieldBool(depObj, config.mqttFieldAC, false);

        // MQTT doesn't provide delay information yet - set to false
        dep.isDelayed = false;
        dep.delayMinutes = 0;

        // Debug log for first few departures (only if debug mode enabled)
        if (config.debugMode && tempCount < 3)
        {
            logTimestamp();
            char debugMsg[128];
            snprintf(debugMsg,
                     sizeof(debugMsg),
                     "MQTT API: Line %s to %s - ETA: %d min (Platform: %s, AC: %d)",
                     dep.line,
                     dep.destination,
                     dep.eta,
                     dep.platform[0] != '\0' ? dep.platform : "",
                     dep.hasAC ? 1 : 0);
            debugPrintln(debugMsg);
        }

        // Add to array
        tempDepartures[tempCount++] = dep;
    }

    logTimestamp();
    debugPrint("MQTT: Successfully parsed ");
    debugPrint(tempCount);
    debugPrintln(" departures");

    return (tempCount > 0);
}

// ============================================================================
// JSON Field Extraction Helpers
// ============================================================================

const char* MqttAPI::getJsonField(JsonObject obj, const char* fieldName, const char* defaultValue)
{
    if (obj.containsKey(fieldName))
    {
        return obj[fieldName].as<const char*>();
    }
    return defaultValue;
}

int MqttAPI::getJsonFieldInt(JsonObject obj, const char* fieldName, int defaultValue)
{
    if (obj.containsKey(fieldName))
    {
        return obj[fieldName].as<int>();
    }
    return defaultValue;
}

bool MqttAPI::getJsonFieldBool(JsonObject obj, const char* fieldName, bool defaultValue)
{
    if (obj.containsKey(fieldName))
    {
        return obj[fieldName].as<bool>();
    }
    return defaultValue;
}
