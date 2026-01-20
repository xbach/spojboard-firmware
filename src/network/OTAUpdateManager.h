#ifndef OTAUPDATEMANAGER_H
#define OTAUPDATEMANAGER_H

#include <WebServer.h>

// ============================================================================
// OTA Update Manager
// ============================================================================

/**
 * Handles Over-The-Air (OTA) firmware updates via web interface.
 * Manages chunked upload, validation, and installation of new firmware.
 */
class OTAUpdateManager
{
  public:
    // Progress callback type (progress bytes, total bytes)
    typedef void (*ProgressCallback)(size_t progress, size_t total);

    OTAUpdateManager();
    ~OTAUpdateManager();

    /**
     * Initialize OTA update system
     * @return true if initialization succeeded
     */
    bool begin();

    /**
     * Handle firmware upload from web server
     * @param server WebServer instance handling the upload
     * @param onProgress Optional callback for progress updates (can be nullptr)
     */
    void handleUpload(WebServer* server, ProgressCallback onProgress);

    /**
     * Check if update is currently in progress
     * @return true if update is active
     */
    bool isUpdating() const
    {
        return updating;
    }

    /**
     * Get last error message
     * @return Error message string (empty if no error)
     */
    const char* getError() const
    {
        return errorMsg;
    }

  private:
    bool updating;
    char errorMsg[128];
    size_t totalSize;
    size_t uploadedSize;
    ProgressCallback progressCallback;

    void setError(const char* msg);
};

#endif // OTAUPDATEMANAGER_H
