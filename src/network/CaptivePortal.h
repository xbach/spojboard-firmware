#ifndef CAPTIVEPORTAL_H
#define CAPTIVEPORTAL_H

#include <DNSServer.h>
#include <WebServer.h>
#include <IPAddress.h>

// ============================================================================
// Captive Portal Manager
// ============================================================================

/**
 * Manages DNS server and captive portal detection for AP mode.
 * Redirects all DNS queries to the AP IP address and handles
 * captive portal detection endpoints for various operating systems.
 */
class CaptivePortal
{
  public:
    CaptivePortal();
    ~CaptivePortal();

    /**
     * Start DNS server for captive portal
     * @param apIP IP address of the access point
     * @return true if DNS server started successfully
     */
    bool begin(IPAddress apIP);

    /**
     * Stop DNS server
     */
    void stop();

    /**
     * Process DNS requests (call from main loop)
     */
    void processRequests();

    /**
     * Register captive portal detection handlers with web server
     * @param server WebServer instance to register handlers with
     */
    void setupDetectionHandlers(WebServer* server);

    /**
     * Check if captive portal is active
     */
    bool isActive() const
    {
        return active;
    }

  private:
    DNSServer dnsServer;
    bool active;
    IPAddress apIP;

    static const byte DNS_PORT = 53;

    // Captive portal detection handler
    static void handleCaptivePortalRedirect(WebServer* server, IPAddress apIP);
};

#endif // CAPTIVEPORTAL_H
