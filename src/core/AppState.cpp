// ============================================================================
// AppState — definitions for the shared application state declared in AppState.h
// ============================================================================
// Order matters: displayController captures displayManager by reference at
// construction, so displayManager MUST be defined first in this translation unit.
// Keeping both here (rather than split across TUs) makes that ordering guaranteed.

#include "core/AppState.h"

// ============================================================================
// Service / hardware objects
// ============================================================================
DisplayManager displayManager;
DisplayController displayController(displayManager);
WiFiManager wifiManager;
CaptivePortal captivePortal;
ConfigWebServer webServer;
GolemioAPI golemioAPI;
BvgAPI bvgAPI;
MqttAPI mqttAPI;
TransitAPI* transitAPI = nullptr;
WeatherAPI weatherAPI;
TickerAPI tickerAPI;

// ============================================================================
// Configuration
// ============================================================================
Config config;

// ============================================================================
// Synchronization primitives
// ============================================================================
TaskHandle_t displayTaskHandle = NULL;
SemaphoreHandle_t displayMutex = NULL;
TaskHandle_t apiFetchTaskHandle = NULL;
SemaphoreHandle_t apiDataMutex = NULL;
SemaphoreHandle_t displayHwMutex = NULL;
SemaphoreHandle_t signalMutex = NULL;
SemaphoreHandle_t configMutex = NULL;

// ============================================================================
// Inter-task request structs
// ============================================================================
DisplayUpdateRequest displayRequest = {.needsUpdate = false};

APIFetchRequest apiFetchRequest = {.fetchDeparturesNow = false,
                                   .fetchWeatherNow = false,
                                   .fetchTickerNow = false,
                                   .lastDeparturesFetch = 0,
                                   .lastWeatherFetch = 0,
                                   .lastTickerFetch = 0,
                                   .departuresRetryPending = false,
                                   .weatherRetryPending = false,
                                   .timezoneInitialized = false};

// ============================================================================
// Shared API data (protected by apiDataMutex)
// ============================================================================
Departure departures[MAX_DEPARTURES];
int departureCount = 0;
WeatherData weatherData = {};
char infoText[TransitAPI::MAX_INFOTEXT_LEN] = "";
TickerData tickerData = {};
char stopName[64] = "";
bool apiError = false;
char apiErrorMsg[64] = "";

// ============================================================================
// Cross-task state flags
// ============================================================================
volatile bool demoModeActive = false;
volatile bool restModeActive = false;
volatile bool restModeManual = false;
volatile bool awaitingDepartures = true;
volatile bool tickerModeActive = false;

// ============================================================================
// Mutex creation (consolidated; called once early in setup())
// ============================================================================
bool createMutexes()
{
    struct
    {
        SemaphoreHandle_t* handle;
        const char* name;
    } muxes[] = {
        {&displayMutex, "Display"},     // first: must exist before any display op
        {&apiDataMutex, "API data"},
        {&displayHwMutex, "Display HW"},
        {&signalMutex, "Signal"},
        {&configMutex, "Config"},
    };

    for (auto& m : muxes)
    {
        *m.handle = xSemaphoreCreateMutex();
        if (*m.handle == NULL)
        {
            Serial.printf("FATAL: Failed to create %s mutex!\n", m.name);
            return false;
        }
        Serial.printf("%s mutex created\n", m.name);
    }
    return true;
}
