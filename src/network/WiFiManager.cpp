#include "WiFiManager.h"
#include "../utils/Logger.h"
#include "../display/DisplayColors.h"
#include <esp_random.h>

WiFiManager::WiFiManager() : apModeActive(false)
{
    apSSID[0] = '\0';
    apPassword[0] = '\0';
}

bool WiFiManager::connectSTA(const Config& config, int maxAttempts, int delayMs)
{
    char msg[96];
    snprintf(msg, sizeof(msg), "WiFi: Connecting to %s", config.wifiSsid);
    logTimestamp();
    debugPrintln(msg);

    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifiSsid, config.wifiPassword);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts)
    {
        delay(delayMs);
        debugPrint(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        snprintf(msg, sizeof(msg), "\nWiFi: Connected! IP: %s", WiFi.localIP().toString().c_str());
        logTimestamp();
        debugPrintln(msg);
        return true;
    }
    else
    {
        logTimestamp();
        debugPrintln("\nWiFi: Connection failed!");
        return false;
    }
}

bool WiFiManager::startAP()
{
    logTimestamp();
    debugPrintln("Starting AP Mode...");

    // Generate credentials
    generateAPName();
    generateRandomPassword();

    // Stop any existing WiFi connection
    WiFi.disconnect(true);
    delay(100);

    // Configure AP
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(apSSID, apPassword))
    {
        logTimestamp();
        debugPrintln("AP Mode failed to start!");
        return false;
    }

    // Configure AP IP (default is 192.168.4.1)
    IPAddress apIP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, gateway, subnet);

    apModeActive = true;

    char msg[128];
    logTimestamp();
    debugPrintln("AP Mode Active!");
    snprintf(msg, sizeof(msg), "  SSID: %s", apSSID);
    debugPrintln(msg);
    snprintf(msg, sizeof(msg), "  Password: %s", apPassword);
    debugPrintln(msg);
    snprintf(msg, sizeof(msg), "  IP: %s", WiFi.softAPIP().toString().c_str());
    debugPrintln(msg);

    return true;
}

void WiFiManager::stopAP()
{
    if (apModeActive)
    {
        logTimestamp();
        debugPrintln("Stopping AP Mode...");

        WiFi.softAPdisconnect(true);
        apModeActive = false;

        delay(100);
    }
}

bool WiFiManager::isConnected() const
{
    return WiFi.status() == WL_CONNECTED;
}

IPAddress WiFiManager::getAPIP() const
{
    return WiFi.softAPIP();
}

int WiFiManager::getAPClientCount() const
{
    return WiFi.softAPgetStationNum();
}

void WiFiManager::attemptReconnect()
{
    if (!apModeActive && WiFi.status() != WL_CONNECTED)
    {
        logTimestamp();
        debugPrintln("WiFi: Attempting reconnection...");
        WiFi.reconnect();
    }
}

void WiFiManager::generateAPName()
{
    // Create unique AP name using last 4 chars of MAC
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(apSSID, sizeof(apSSID), "%s%02X%02X", AP_SSID_PREFIX, mac[4], mac[5]);
}

void WiFiManager::generateRandomPassword()
{
    // Generate 8-character alphanumeric password
    const char charset[] = "abcdefghjkmnpqrstuvwxyz23456789"; // Excluded confusing chars: i,l,o,0,1
    randomSeed(esp_random()); // Use hardware RNG

    for (int i = 0; i < 8; i++)
    {
        apPassword[i] = charset[random(0, strlen(charset))];
    }
    apPassword[8] = '\0';
}
