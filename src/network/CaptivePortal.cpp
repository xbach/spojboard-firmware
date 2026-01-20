#include "CaptivePortal.h"
#include "../utils/Logger.h"

CaptivePortal::CaptivePortal() : active(false), apIP(0, 0, 0, 0) {}

CaptivePortal::~CaptivePortal()
{
    stop();
}

bool CaptivePortal::begin(IPAddress ip)
{
    apIP = ip;

    // Start DNS server (redirect all domains to AP IP)
    if (!dnsServer.start(DNS_PORT, "*", apIP))
    {
        logTimestamp();
        Serial.println("DNS Server failed to start!");
        return false;
    }

    active = true;

    logTimestamp();
    Serial.print("DNS Server started on port ");
    Serial.println(DNS_PORT);

    return true;
}

void CaptivePortal::stop()
{
    if (active)
    {
        dnsServer.stop();
        active = false;

        logTimestamp();
        Serial.println("DNS Server stopped");
    }
}

void CaptivePortal::processRequests()
{
    if (active)
    {
        dnsServer.processNextRequest();
    }
}

void CaptivePortal::handleCaptivePortalRedirect(WebServer* server, IPAddress apIP)
{
    char redirectUrl[32];
    snprintf(redirectUrl, sizeof(redirectUrl), "http://%d.%d.%d.%d/", apIP[0], apIP[1], apIP[2], apIP[3]);

    server->sendHeader("Location", redirectUrl);
    server->send(302, "text/plain", "");
}

void CaptivePortal::setupDetectionHandlers(WebServer* server)
{
    // Store apIP for use in lambda captures
    IPAddress ip = apIP;

    // Android captive portal detection
    server->on("/generate_204", [server, ip]() { handleCaptivePortalRedirect(server, ip); });
    server->on("/gen_204", [server, ip]() { handleCaptivePortalRedirect(server, ip); });

    // iOS/macOS captive portal detection
    server->on("/hotspot-detect.html", [server, ip]() { handleCaptivePortalRedirect(server, ip); });
    server->on("/library/test/success.html", [server, ip]() { handleCaptivePortalRedirect(server, ip); });

    // Windows captive portal detection
    server->on("/ncsi.txt", [server, ip]() { handleCaptivePortalRedirect(server, ip); });
    server->on("/connecttest.txt", [server, ip]() { handleCaptivePortalRedirect(server, ip); });
    server->on("/redirect", [server, ip]() { handleCaptivePortalRedirect(server, ip); });

    // Firefox captive portal detection
    server->on("/success.txt", [server, ip]() { handleCaptivePortalRedirect(server, ip); });

    logTimestamp();
    Serial.println("Captive portal detection handlers registered");
}
