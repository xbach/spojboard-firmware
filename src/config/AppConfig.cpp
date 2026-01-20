#include "AppConfig.h"
#include "../utils/Logger.h"
#include <Arduino.h>

void loadConfig(Config& config)
{
    Preferences preferences;
    preferences.begin("transport", true); // Read-only

    strlcpy(config.wifiSsid, preferences.getString("wifiSsid", DEFAULT_WIFI_SSID).c_str(), sizeof(config.wifiSsid));
    strlcpy(config.wifiPassword, preferences.getString("wifiPass", DEFAULT_WIFI_PASSWORD).c_str(), sizeof(config.wifiPassword));

    // Load per-city configuration fields
    strlcpy(config.pragueApiKey, preferences.getString("pragueApiKey", "").c_str(), sizeof(config.pragueApiKey));
    strlcpy(config.pragueStopIds, preferences.getString("pragueStopIds", "U693Z2P").c_str(), sizeof(config.pragueStopIds));
    strlcpy(config.berlinStopIds, preferences.getString("berlinStopIds", "").c_str(), sizeof(config.berlinStopIds));

    // Load MQTT-specific configuration fields
    strlcpy(config.mqttBroker, preferences.getString("mqttBroker", "").c_str(), sizeof(config.mqttBroker));
    config.mqttPort = preferences.getInt("mqttPort", 1883);
    strlcpy(config.mqttUsername, preferences.getString("mqttUser", "").c_str(), sizeof(config.mqttUsername));
    strlcpy(config.mqttPassword, preferences.getString("mqttPass", "").c_str(), sizeof(config.mqttPassword));
    strlcpy(config.mqttRequestTopic, preferences.getString("mqttReqTopic", "").c_str(), sizeof(config.mqttRequestTopic));
    strlcpy(config.mqttResponseTopic, preferences.getString("mqttRespTopic", "").c_str(), sizeof(config.mqttResponseTopic));
    config.mqttUseEtaMode = preferences.getBool("mqttEtaMode", false);  // Default: timestamp mode

    // Load MQTT JSON field mappings with sensible defaults
    strlcpy(config.mqttFieldLine, preferences.getString("mqttFldLine", "line").c_str(), sizeof(config.mqttFieldLine));
    strlcpy(config.mqttFieldDestination, preferences.getString("mqttFldDest", "dest").c_str(), sizeof(config.mqttFieldDestination));
    strlcpy(config.mqttFieldEta, preferences.getString("mqttFldEta", "eta").c_str(), sizeof(config.mqttFieldEta));
    strlcpy(config.mqttFieldTimestamp, preferences.getString("mqttFldTime", "dep").c_str(), sizeof(config.mqttFieldTimestamp));
    strlcpy(config.mqttFieldPlatform, preferences.getString("mqttFldPlat", "plt").c_str(), sizeof(config.mqttFieldPlatform));
    strlcpy(config.mqttFieldAC, preferences.getString("mqttFldAC", "ac").c_str(), sizeof(config.mqttFieldAC));

    // Backward compatibility: Migrate old config format to new per-city fields
    // If old fields exist and new fields are empty, migrate the data
    if (preferences.isKey("apiKey") && strlen(config.pragueApiKey) == 0)
    {
        String oldApiKey = preferences.getString("apiKey", "");
        if (oldApiKey.length() > 0)
        {
            strlcpy(config.pragueApiKey, oldApiKey.c_str(), sizeof(config.pragueApiKey));
            Serial.println("  Migrated old apiKey to pragueApiKey");
        }
    }
    if (preferences.isKey("stopIds"))
    {
        String oldStopIds = preferences.getString("stopIds", "");
        if (oldStopIds.length() > 0)
        {
            // Migrate to Prague by default (backward compatibility)
            if (strlen(config.pragueStopIds) == 0 || strcmp(config.pragueStopIds, "U693Z2P") == 0)
            {
                strlcpy(config.pragueStopIds, oldStopIds.c_str(), sizeof(config.pragueStopIds));
                Serial.println("  Migrated old stopIds to pragueStopIds");
            }
        }
    }

    config.refreshInterval = preferences.getInt("refresh", 60);  // Increased to 60s to reduce API load
    config.numDepartures = preferences.getInt("numDeps", 3);     // Display rows (1-3)
    config.minDepartureTime = preferences.getInt("minDepTime", 3);
    config.brightness = preferences.getInt("brightness", 90);
    strlcpy(config.lineColorMap, preferences.getString("lineColorMap", "").c_str(), sizeof(config.lineColorMap));
    strlcpy(config.city, preferences.getString("city", "Prague").c_str(), sizeof(config.city));  // Default: Prague for backward compatibility
    strlcpy(config.language, preferences.getString("language", "en").c_str(), sizeof(config.language));  // Default: English
    config.debugMode = preferences.getBool("debugMode", false);  // Default: disabled
    config.showPlatform = preferences.getBool("showPlatform", false);  // Default: disabled
    config.scrollEnabled = preferences.getBool("scrollEnabled", false);  // Default: disabled
    strlcpy(config.restModePeriods, preferences.getString("restPeriods", "").c_str(), sizeof(config.restModePeriods));

    // Load weather configuration
    config.weatherEnabled = preferences.getBool("weatherEnable", false);  // Default: disabled
    config.weatherLatitude = preferences.getFloat("weatherLat", 50.0755);  // Default: Prague
    config.weatherLongitude = preferences.getFloat("weatherLon", 14.4378); // Default: Prague
    config.weatherRefreshInterval = preferences.getInt("weatherRefresh", 15);  // Default: 15 minutes

    config.configured = preferences.getBool("configured", false);

    preferences.end();

    // Config loading happens before logger init, so use Serial directly
    logTimestamp();
    Serial.println("Config loaded:");
    Serial.print("  SSID: ");
    Serial.println(config.wifiSsid);
    Serial.print("  City: ");
    Serial.println(config.city);
    Serial.print("  Prague API Key: ");
    Serial.println(strlen(config.pragueApiKey) > 0 ? "Configured" : "Not set");
    Serial.print("  Prague Stops: ");
    Serial.println(config.pragueStopIds);
    Serial.print("  Berlin Stops: ");
    Serial.println(strlen(config.berlinStopIds) > 0 ? config.berlinStopIds : "Not set");
    Serial.print("  MQTT Broker: ");
    Serial.println(strlen(config.mqttBroker) > 0 ? config.mqttBroker : "Not set");
    Serial.print("  Refresh: ");
    Serial.print(config.refreshInterval);
    Serial.println("s");
    Serial.print("  Configured: ");
    Serial.println(config.configured ? "Yes" : "No");
}

void saveConfig(const Config& config)
{
    Preferences preferences;
    preferences.begin("transport", false); // Read-write

    preferences.putString("wifiSsid", config.wifiSsid);
    preferences.putString("wifiPass", config.wifiPassword);

    // Save per-city configuration fields
    preferences.putString("pragueApiKey", config.pragueApiKey);
    preferences.putString("pragueStopIds", config.pragueStopIds);
    preferences.putString("berlinStopIds", config.berlinStopIds);

    // Save MQTT-specific configuration fields
    preferences.putString("mqttBroker", config.mqttBroker);
    preferences.putInt("mqttPort", config.mqttPort);
    preferences.putString("mqttUser", config.mqttUsername);
    preferences.putString("mqttPass", config.mqttPassword);
    preferences.putString("mqttReqTopic", config.mqttRequestTopic);
    preferences.putString("mqttRespTopic", config.mqttResponseTopic);
    preferences.putBool("mqttEtaMode", config.mqttUseEtaMode);

    // Save MQTT JSON field mappings
    preferences.putString("mqttFldLine", config.mqttFieldLine);
    preferences.putString("mqttFldDest", config.mqttFieldDestination);
    preferences.putString("mqttFldEta", config.mqttFieldEta);
    preferences.putString("mqttFldTime", config.mqttFieldTimestamp);
    preferences.putString("mqttFldPlat", config.mqttFieldPlatform);
    preferences.putString("mqttFldAC", config.mqttFieldAC);

    // Remove old keys if they exist (cleanup after migration)
    if (preferences.isKey("apiKey"))
    {
        preferences.remove("apiKey");
    }
    if (preferences.isKey("stopIds"))
    {
        preferences.remove("stopIds");
    }

    preferences.putInt("refresh", config.refreshInterval);
    preferences.putInt("numDeps", config.numDepartures);
    preferences.putInt("minDepTime", config.minDepartureTime);
    preferences.putInt("brightness", config.brightness);
    preferences.putString("lineColorMap", config.lineColorMap);
    preferences.putString("city", config.city);
    preferences.putString("language", config.language);
    preferences.putBool("debugMode", config.debugMode);
    preferences.putBool("showPlatform", config.showPlatform);
    preferences.putBool("scrollEnabled", config.scrollEnabled);
    preferences.putString("restPeriods", config.restModePeriods);

    // Save weather configuration
    preferences.putBool("weatherEnable", config.weatherEnabled);
    preferences.putFloat("weatherLat", config.weatherLatitude);
    preferences.putFloat("weatherLon", config.weatherLongitude);
    preferences.putInt("weatherRefresh", config.weatherRefreshInterval);

    preferences.putBool("configured", true);

    preferences.end();

    logTimestamp();
    debugPrintln("Config saved");
}

void clearConfig()
{
    Preferences preferences;
    preferences.begin("transport", false); // Read-write

    // Clear all keys in the namespace
    preferences.clear();

    preferences.end();

    logTimestamp();
    debugPrintln("All configuration cleared - device will boot into AP mode on restart");
}
