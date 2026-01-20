#ifndef TELNETLOGGER_H
#define TELNETLOGGER_H

#include <ESPTelnet.h>

// ============================================================================
// Telnet Logger - Remote serial monitoring over WiFi
// ============================================================================

/**
 * Singleton telnet logger for remote debugging
 * Mirrors Serial output to telnet clients when debug mode is enabled
 */
class TelnetLogger
{
  public:
    /**
     * Get singleton instance
     */
    static TelnetLogger& getInstance();

    /**
     * Initialize telnet server
     * @param port Telnet port (default: 23)
     * @return true if started successfully
     */
    bool begin(uint16_t port = 23);

    /**
     * Process telnet client connections (call in loop)
     */
    void loop();

    /**
     * Print message to telnet clients (if any connected)
     * @param message Message to send
     */
    void print(const char* message);

    /**
     * Print message with newline to telnet clients
     * @param message Message to send
     */
    void println(const char* message);

    /**
     * Check if telnet server is running
     */
    bool isActive();

    /**
     * Check if any clients are connected
     */
    bool hasClients();

    /**
     * Stop telnet server
     */
    void end();

  private:
    TelnetLogger();
    TelnetLogger(const TelnetLogger&) = delete;
    TelnetLogger& operator=(const TelnetLogger&) = delete;

    ESPTelnet telnet;
    bool active;
};

#endif // TELNETLOGGER_H
