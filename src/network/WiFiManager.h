#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <WiFi.h>
#include "../config/AppConfig.h"

// ============================================================================
// WiFi Manager
// ============================================================================

/**
 * Manages WiFi connections in both Station (STA) and Access Point (AP) modes.
 * Handles connection attempts, auto-fallback to AP mode, and AP credential generation.
 */
class WiFiManager
{
  public:
    WiFiManager();

    /**
     * Attempt to connect to WiFi in Station mode
     * @param config Configuration with WiFi credentials
     * @param maxAttempts Maximum connection attempts (default: 20)
     * @param delayMs Delay between attempts in milliseconds (default: 500)
     * @return true if connected successfully
     */
    bool connectSTA(const Config& config, int maxAttempts = 20, int delayMs = 500);

    /**
     * Start Access Point mode with auto-generated credentials
     * @return true if AP mode started successfully
     */
    bool startAP();

    /**
     * Stop Access Point mode
     */
    void stopAP();

    /**
     * Check if currently in AP mode
     */
    bool isAPMode() const
    {
        return apModeActive;
    }

    /**
     * Check if WiFi is connected (STA mode)
     */
    bool isConnected() const;

    /**
     * Get AP SSID (only valid if in AP mode)
     */
    const char* getAPSSID() const
    {
        return apSSID;
    }

    /**
     * Get AP password (only valid if in AP mode)
     */
    const char* getAPPassword() const
    {
        return apPassword;
    }

    /**
     * Get AP IP address
     */
    IPAddress getAPIP() const;

    /**
     * Get number of clients connected to AP
     */
    int getAPClientCount() const;

    /**
     * Attempt to reconnect to WiFi (call periodically if disconnected)
     */
    void attemptReconnect();

    /**
     * Device code: last 3 bytes of the base MAC as uppercase hex ("9B9D2C").
     *
     * The single source of every name this device answers to -- hostname, AP
     * SSID (which uses the last 4 chars) and MQTT client ID all derive from
     * this, so they cannot drift apart.
     *
     * Reads the MAC with esp_read_mac(), deliberately NOT WiFi.macAddress():
     * the latter resolves to NetworkInterface::macAddress(), which returns
     * NULL and leaves the caller's buffer UNTOUCHED when the STA netif does
     * not exist yet. The hostname has to be set before WiFi.mode(WIFI_STA)
     * creates that netif (see connectSTA), so a netif-dependent MAC read is
     * unusable here. esp_read_mac() reads eFuse and works before WiFi init.
     *
     * Cached in a function-local static. Primed from setup() context (every
     * path runs connectSTA first), so it is never first-written concurrently.
     */
    static const char* getDeviceCode();

    /**
     * Network hostname ("spojboard-9B9D2C"). This is what the DHCP client
     * announces as option 12, i.e. what a router's client list displays,
     * replacing the Arduino core default of "esp32s3-<mac[3..5]>".
     */
    static const char* getHostname();

  private:
    bool apModeActive;
    char apSSID[32];
    char apPassword[9]; // 8 chars + null terminator

    static constexpr const char* AP_SSID_PREFIX = "SpojBoard-";
    static constexpr const char* HOSTNAME_PREFIX = "spojboard-";

    void generateAPName();
    void generateRandomPassword();
};

#endif // WIFIMANAGER_H
