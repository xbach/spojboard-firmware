#include "WiFiManager.h"
#include "../utils/Logger.h"
#include "../display/DisplayColors.h"
#include <esp_random.h>
#include <esp_mac.h>

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

    // ORDER IS LOAD-BEARING: the hostname must be set BEFORE WiFi.mode(WIFI_STA).
    // WiFi.setHostname() only writes a global string; the core copies that string
    // into the STA netif exactly once, on the transition INTO STA mode
    // (WiFiGeneric.cpp calls esp_netif_set_hostname when STA was not previously
    // enabled). Set it afterwards and it silently does nothing for the rest of the
    // session -- WiFi.getHostname() reports the new value while DHCP keeps
    // announcing the core default -- which reads as "setHostname is broken".
    WiFi.setHostname(getHostname());
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
    // Last 4 chars of the 6-char device code == the old mac[4],mac[5] pair, so the
    // SSID is byte-identical to what shipped before -- but it now comes from the
    // same MAC read as the hostname and the MQTT client ID instead of a separate
    // WiFi.macAddress() call that only works once the STA netif exists.
    snprintf(apSSID, sizeof(apSSID), "%s%s", AP_SSID_PREFIX, getDeviceCode() + 2);
}

const char* WiFiManager::getDeviceCode()
{
    static char code[7] = {0}; // 6 hex chars + null

    if (code[0] == '\0')
    {
        uint8_t mac[6] = {0};
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(code, sizeof(code), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    }

    return code;
}

const char* WiFiManager::getHostname()
{
    static char hostname[32] = {0}; // esp_netif_set_hostname caps at 32 bytes

    if (hostname[0] == '\0')
    {
        snprintf(hostname, sizeof(hostname), "%s%s", HOSTNAME_PREFIX, getDeviceCode());
    }

    return hostname;
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
