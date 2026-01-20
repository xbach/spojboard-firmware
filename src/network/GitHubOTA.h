#ifndef GITHUBOTA_H
#define GITHUBOTA_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "OTAUpdateManager.h"

// ============================================================================
// GitHub OTA Update Manager
// ============================================================================

/**
 * Handles checking for firmware updates from GitHub releases and downloading them.
 * Integrates with OTAUpdateManager for the actual flashing process.
 */
class GitHubOTA
{
  public:
    struct ReleaseInfo
    {
        bool available; // Is update available?
        bool hasError; // API error occurred?
        char errorMsg[128]; // Error message
        int releaseNumber; // Numeric release (e.g., 1, 2, 3)
        char tagName[32]; // Tag name (e.g., "r1", "r2")
        char releaseName[64]; // Human-readable name (e.g., "Release 1")
        char releaseNotes[512]; // Truncated release body
        char assetUrl[256]; // Download URL for .bin file
        char assetName[64]; // Filename
        size_t assetSize; // File size in bytes
    };

    // Progress callback type (progress bytes, total bytes)
    typedef void (*ProgressCallback)(size_t progress, size_t total);

    GitHubOTA();
    ~GitHubOTA();

    /**
     * Check GitHub for latest release
     * Compares current FIRMWARE_RELEASE with GitHub tag
     * @param currentRelease Current firmware release number string
     * @return ReleaseInfo with update availability and details
     */
    ReleaseInfo checkForUpdate(const char* currentRelease);

    /**
     * Download and install firmware from GitHub release
     * @param assetUrl GitHub asset download URL
     * @param expectedSize Expected file size (for validation)
     * @param onProgress Progress callback (bytes downloaded, total bytes)
     * @return true if download and flash succeeded
     */
    bool downloadAndInstall(const char* assetUrl, size_t expectedSize, ProgressCallback onProgress);

  private:
    static constexpr int JSON_BUFFER_SIZE = 8192;
    static constexpr int HTTP_TIMEOUT_MS = 30000; // 30 seconds for downloads
    static constexpr const char* GITHUB_API_URL =
        "https://api.github.com/repos/xbach/spojboard-firmware/releases/latest";

    /**
     * Parse release number from tag name (e.g., "r1" -> 1)
     * @param tagName GitHub tag name
     * @return Release number or -1 if invalid
     */
    int parseReleaseNumber(const char* tagName);

    /**
     * Find .bin asset in release
     * @param doc ArduinoJson document with release data
     * @param outUrl Output buffer for download URL
     * @param outName Output buffer for filename
     * @param outSize Output for file size
     * @return true if .bin asset found
     */
    bool findBinaryAsset(JsonDocument& doc, char* outUrl, char* outName, size_t& outSize);

    /**
     * Validate firmware filename format
     * @param filename Filename to check (e.g., "spojboard-r2-a1b2c3d4.bin")
     * @return true if format is valid
     */
    bool validateFirmwareFilename(const char* filename);

    /**
     * Set error message
     * @param msg Error message to set
     */
    void setError(ReleaseInfo& info, const char* msg);
};

#endif // GITHUBOTA_H
