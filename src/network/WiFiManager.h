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

  private:
    bool apModeActive;
    char apSSID[32];
    char apPassword[9]; // 8 chars + null terminator

    static constexpr const char* AP_SSID_PREFIX = "SpojBoard-";

    void generateAPName();
    void generateRandomPassword();
};

#endif // WIFIMANAGER_H
