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

    // Display hardware profile (TA-0302). Absent keys mean a device updating
    // from an older release boots on the compiled defaults, unchanged.
    config.hwProfile.useCustomPins = preferences.getBool("hwCustom", false);
    config.hwProfile.pins = hwCompiledDefaultPins();
    if (preferences.isKey("hwPins"))
    {
        HubPins stored;
        if (preferences.getBytes("hwPins", &stored, sizeof(stored)) == sizeof(stored))
        {
            config.hwProfile.pins = stored;
        }
    }
    {
        // An out-of-range stored order would index past the permutation table.
        const int order = preferences.getInt("hwRgbOrder", (int)DEFAULT_RGB_ORDER);
        config.hwProfile.order = (order >= 0 && order <= (int)RgbOrder::BGR) ? (RgbOrder)order : DEFAULT_RGB_ORDER;
    }
    config.hwProfile.driver = (uint8_t)constrain(preferences.getInt("hwDriver", 0), 0, 5);

    config.refreshInterval = constrain(preferences.getInt("refresh", 60), 10, 300);
    config.minDepartureTime = constrain(preferences.getInt("minDepTime", 3), 0, 30);
    config.brightness = constrain(preferences.getInt("brightness", 90), 0, 255);
    config.panelRows = constrain(preferences.getInt("panelRows", 1), 1, 2);
#if DISPLAY_VARIANT != 1
    // Geometry-specific build: the compiled panel arrangement is the truth, so
    // the stored value only decides how many rows the user gets, and it cannot
    // be allowed to disagree with the panel that is physically attached.
    //
    // Deliberately NOT applied on the default 2x32 build. TA-0269 §7 detects a
    // 4-panel owner who landed on a 2x32 image by exactly one condition --
    // compiled variant is 2x32 AND stored panelRows == 2 -- and saveConfig()
    // writes this field back, so forcing it here on a 2x32 build would erase the
    // only surviving evidence of what hardware the device actually has.
    config.panelRows = DISPLAY_PANEL_ROWS;
#endif
    int maxDeps = (config.panelRows * 32 / 8) - 1; // 3 for 128x32, 7 for 128x64
    config.numDepartures = constrain(preferences.getInt("numDeps", 3), 1, maxDeps);
    strlcpy(config.lineColorMap, preferences.getString("lineColorMap", DEFAULT_LINE_COLOR_MAP).c_str(), sizeof(config.lineColorMap));
    if (strlen(config.lineColorMap) == 0)
    {
        strlcpy(config.lineColorMap, DEFAULT_LINE_COLOR_MAP, sizeof(config.lineColorMap));
    }
    strlcpy(config.platformSymbolMap, preferences.getString("pltSymMap", "").c_str(), sizeof(config.platformSymbolMap));
    strlcpy(config.city, preferences.getString("city", "Prague").c_str(), sizeof(config.city));  // Default: Prague for backward compatibility
    strlcpy(config.language, preferences.getString("language", "en").c_str(), sizeof(config.language));  // Default: English
    config.debugMode = preferences.getBool("debugMode", false);  // Default: disabled
    config.showPlatform = preferences.getBool("showPlatform", false);  // Default: disabled
    config.scrollEnabled = preferences.getBool("scrollEnabled", false);  // Default: disabled
    config.showMultipleTimes = preferences.getBool("showMultiEta", false);  // Default: disabled
    strlcpy(config.restModePeriods, preferences.getString("restPeriods", "").c_str(), sizeof(config.restModePeriods));

    // Load weather configuration
    config.weatherEnabled = preferences.getBool("weatherEnable", false);  // Default: disabled
    config.weatherLatitude = preferences.getFloat("weatherLat", 50.0755);  // Default: Prague
    config.weatherLongitude = preferences.getFloat("weatherLon", 14.4378); // Default: Prague
    config.weatherRefreshInterval = constrain(preferences.getInt("weatherRefresh", 15), 5, 120);

    // Load ticker mode configuration
    config.tickerEnabled = preferences.getBool("tickerOn", false);
    strlcpy(config.tickerSymbol, preferences.getString("tickerSymbol", "BTC/USD").c_str(), sizeof(config.tickerSymbol));
    strlcpy(config.tickerInterval, preferences.getString("tickerIntvl", "1day").c_str(), sizeof(config.tickerInterval));
    strlcpy(config.tickerApiKey, preferences.getString("tickerApiKey", "").c_str(), sizeof(config.tickerApiKey));
    config.tickerRefreshInterval = constrain(preferences.getInt("tickerRefresh", 120), 120, 600);

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
    Serial.print("  Panel rows: ");
    Serial.println(config.panelRows);
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

    preferences.putBool("hwCustom", config.hwProfile.useCustomPins);
    preferences.putBytes("hwPins", &config.hwProfile.pins, sizeof(config.hwProfile.pins));
    preferences.putInt("hwRgbOrder", (int)config.hwProfile.order);
    preferences.putInt("hwDriver", config.hwProfile.driver);

    preferences.putInt("refresh", config.refreshInterval);
    preferences.putInt("numDeps", config.numDepartures);
    preferences.putInt("minDepTime", config.minDepartureTime);
    preferences.putInt("brightness", config.brightness);
    preferences.putInt("panelRows", config.panelRows);
    preferences.putString("lineColorMap", config.lineColorMap);
    preferences.putString("pltSymMap", config.platformSymbolMap);
    preferences.putString("city", config.city);
    preferences.putString("language", config.language);
    preferences.putBool("debugMode", config.debugMode);
    preferences.putBool("showPlatform", config.showPlatform);
    preferences.putBool("scrollEnabled", config.scrollEnabled);
    preferences.putBool("showMultiEta", config.showMultipleTimes);
    preferences.putString("restPeriods", config.restModePeriods);

    // Save weather configuration
    preferences.putBool("weatherEnable", config.weatherEnabled);
    preferences.putFloat("weatherLat", config.weatherLatitude);
    preferences.putFloat("weatherLon", config.weatherLongitude);
    preferences.putInt("weatherRefresh", config.weatherRefreshInterval);

    // Save ticker mode configuration
    preferences.putBool("tickerOn", config.tickerEnabled);
    preferences.putString("tickerSymbol", config.tickerSymbol);
    preferences.putString("tickerIntvl", config.tickerInterval);
    preferences.putString("tickerApiKey", config.tickerApiKey);
    preferences.putInt("tickerRefresh", config.tickerRefreshInterval);

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

bool verifyHardware()
{
    Preferences preferences;
    preferences.begin("transport", true); // Read-only first
    int storedVariant = preferences.getInt("hw_variant", -1);
    preferences.end();

    if (storedVariant == -1)
    {
        // First boot - store the hardware variant
        preferences.begin("transport", false); // Read-write
        preferences.putInt("hw_variant", FIRMWARE_VARIANT);
        preferences.end();

        Serial.println("===========================================");
        Serial.printf("First boot: Hardware variant stored: %s (%d)\n",
                      VARIANT_DISPLAY_NAME, FIRMWARE_VARIANT);
        Serial.println("===========================================");
        return true;
    }

    // Check if stored variant matches compiled variant
    if (storedVariant != FIRMWARE_VARIANT)
    {
        Serial.println("===========================================");
        Serial.println("FATAL ERROR: FIRMWARE-HARDWARE MISMATCH!");
        Serial.println("===========================================");
        Serial.printf("This firmware is for: %s (variant %d)\n",
                      VARIANT_DISPLAY_NAME, FIRMWARE_VARIANT);
        Serial.printf("This device was initialized with variant %d\n", storedVariant);
        Serial.println("");
        Serial.println("DO NOT proceed - this prevents potential damage.");
        Serial.println("Flash the correct firmware for your hardware.");
        Serial.println("===========================================");
        return false;
    }

    // Variant matches
    Serial.printf("Hardware verification passed: %s (variant %d)\n",
                  VARIANT_DISPLAY_NAME, FIRMWARE_VARIANT);
    return true;
}

