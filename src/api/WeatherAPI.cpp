#include "WeatherAPI.h"
#include "../utils/Logger.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

WeatherAPI::WeatherAPI()
{
}

WeatherData WeatherAPI::fetchWeather(float latitude, float longitude)
{
    WeatherData result = {};
    result.hasError = false;
    result.temperature = 0;
    result.weatherCode = 0;
    time(&result.timestamp);

    logTimestamp();
    debugPrintln("Weather: Starting fetch...");
    logMemory("weather_start");

    // Validate coordinates
    if (latitude < -90.0 || latitude > 90.0 || longitude < -180.0 || longitude > 180.0)
    {
        result.hasError = true;
        strlcpy(result.errorMsg, "Invalid coordinates", sizeof(result.errorMsg));
        logTimestamp();
        debugPrintln("Weather: Invalid coordinates provided");
        return result;
    }

    // Build API URL
    char url[256];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&hourly=temperature_2m,weathercode&forecast_hours=3&timezone=auto",
             latitude, longitude);

    logTimestamp();
    debugPrint("Weather: URL: ");
    debugPrintln(url);

    HTTPClient http;
    int httpCode = -1;
    bool success = false;

    // Retry logic (up to MAX_RETRIES attempts)
    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++)
    {
        if (attempt > 1)
        {
            int delayMs = 2000 * (attempt - 1); // 2s, 4s...
            logTimestamp();
            debugPrint("Weather: Retry ");
            debugPrint(String(attempt - 1).c_str());
            debugPrint("/");
            debugPrintln(String(MAX_RETRIES - 1).c_str());
            delay(delayMs);
        }

        http.begin(url);
        http.setTimeout(HTTP_TIMEOUT_MS);

        logTimestamp();
        debugPrintln("Weather: Sending HTTP GET...");

        httpCode = http.GET();

        logTimestamp();
        debugPrint("Weather: HTTP code: ");
        debugPrintln(String(httpCode).c_str());

        if (httpCode == HTTP_CODE_OK)
        {
            success = true;
            break;
        }

        // Don't retry on 4xx client errors
        if (httpCode >= 400 && httpCode < 500)
        {
            break;
        }
    }

    if (!success || httpCode != HTTP_CODE_OK)
    {
        result.hasError = true;
        snprintf(result.errorMsg, sizeof(result.errorMsg), "HTTP error: %d", httpCode);
        http.end();
        logTimestamp();
        debugPrint("Weather: Fetch failed with HTTP code: ");
        debugPrintln(String(httpCode).c_str());
        return result;
    }

    // Parse JSON response
    String payload = http.getString();
    http.end();

    logTimestamp();
    debugPrint("Weather: Response size: ");
    debugPrint(String(payload.length()).c_str());
    debugPrintln(" bytes");

    DynamicJsonDocument doc(JSON_BUFFER_SIZE);
    DeserializationError error = deserializeJson(doc, payload);

    if (error)
    {
        result.hasError = true;
        strlcpy(result.errorMsg, "JSON parse error", sizeof(result.errorMsg));
        logTimestamp();
        debugPrint("Weather: JSON parse error: ");
        debugPrintln(error.c_str());
        return result;
    }

    // Extract hourly data (first entry = current/3-hour forecast)
    if (!doc.containsKey("hourly") || !doc["hourly"].containsKey("temperature_2m") ||
        !doc["hourly"].containsKey("weathercode"))
    {
        result.hasError = true;
        strlcpy(result.errorMsg, "Missing hourly data", sizeof(result.errorMsg));
        logTimestamp();
        debugPrintln("Weather: Response missing expected fields");
        return result;
    }

    JsonArray temps = doc["hourly"]["temperature_2m"];
    JsonArray codes = doc["hourly"]["weathercode"];

    if (temps.size() == 0 || codes.size() == 0)
    {
        result.hasError = true;
        strlcpy(result.errorMsg, "Empty hourly arrays", sizeof(result.errorMsg));
        logTimestamp();
        debugPrintln("Weather: Empty hourly data arrays");
        return result;
    }

    // Get first entry (3-hour forecast)
    float tempFloat = temps[0];
    result.temperature = (int)round(tempFloat);
    result.weatherCode = codes[0];

    logTimestamp();
    char msg[64];
    snprintf(msg, sizeof(msg), "Weather: Success - %d°C, WMO code %d", result.temperature, result.weatherCode);
    debugPrintln(msg);
    logMemory("weather_complete");

    return result;
}
