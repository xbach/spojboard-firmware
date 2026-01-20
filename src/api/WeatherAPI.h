#ifndef WEATHERAPI_H
#define WEATHERAPI_H

#include <time.h>

// ============================================================================
// Weather Data Structures
// ============================================================================

/**
 * Weather data structure for current/forecast conditions
 */
struct WeatherData {
    int temperature;       // Temperature in Celsius (e.g., 15 = 15°C)
    int weatherCode;       // WMO weather code (0-99)
    time_t timestamp;      // Unix timestamp when this data was fetched
    bool hasError;         // True if fetch encountered an error
    char errorMsg[64];     // Error message if hasError is true
};

// ============================================================================
// WeatherAPI Client
// ============================================================================

/**
 * Client for Open-Meteo API (open-meteo.com)
 * Fetches 3-hour weather forecast for display.
 * No API key required - free and open source weather service.
 */
class WeatherAPI
{
public:
    WeatherAPI();

    /**
     * Fetch 3-hour weather forecast from Open-Meteo API
     * @param latitude Location latitude (e.g., 50.0755 for Prague)
     * @param longitude Location longitude (e.g., 14.4378 for Prague)
     * @return WeatherData struct with temperature, weather code, and error status
     */
    WeatherData fetchWeather(float latitude, float longitude);

private:
    static constexpr int JSON_BUFFER_SIZE = 2048;  // 2KB buffer for Open-Meteo response
    static constexpr int HTTP_TIMEOUT_MS = 8000;   // 8 second timeout
    static constexpr int MAX_RETRIES = 2;          // Fewer retries than transit APIs
};

#endif // WEATHERAPI_H
