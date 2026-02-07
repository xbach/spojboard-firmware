#include "GitHubOTA.h"
#include "../config/AppConfig.h"
#include "../utils/Logger.h"
#include <Update.h>
#include <WiFi.h>

GitHubOTA::GitHubOTA() {}

GitHubOTA::~GitHubOTA() {}

void GitHubOTA::setError(ReleaseInfo& info, const char* msg)
{
    info.hasError = true;
    strlcpy(info.errorMsg, msg, sizeof(info.errorMsg));
}

int GitHubOTA::parseReleaseNumber(const char* tagName)
{
    // Expected format: "r1", "r2", "r10", etc.
    if (!tagName || tagName[0] != 'r')
    {
        return -1;
    }

    // Parse the number after 'r'
    int releaseNum = atoi(tagName + 1);
    if (releaseNum <= 0)
    {
        return -1;
    }

    return releaseNum;
}

bool GitHubOTA::extractVariantFromFilename(const char* filename, char* variant, size_t variantSize)
{
    // Expected format: spojboard-{variant}-r{number}-{8hex}.bin
    // Example: spojboard-matrixportal_s3-r4-a1b2c3d4.bin

    if (!filename || !variant)
    {
        return false;
    }

    // Find "spojboard-" prefix
    const char* start = strstr(filename, "spojboard-");
    if (!start)
    {
        return false;
    }

    start += 10; // Skip "spojboard-"

    // Find next "-r" which marks end of variant
    const char* end = strstr(start, "-r");
    if (!end)
    {
        return false;
    }

    // Extract variant name
    int len = end - start;
    if (len <= 0 || (size_t)len >= variantSize)
    {
        return false;
    }

    strncpy(variant, start, len);
    variant[len] = '\0';
    return true;
}

bool GitHubOTA::validateFirmwareFilename(const char* filename)
{
    // Expected formats:
    // New: spojboard-{variant}-r{number}-{8hex}.bin (e.g., spojboard-matrixportal_s3-r4-a1b2c3d4.bin)
    // Old: spojboard-r{number}-{8hex}.bin (e.g., spojboard-r4-1bc62fce.bin) - backward compatibility

    if (!filename)
    {
        return false;
    }

    // Check prefix
    if (strncmp(filename, "spojboard-", 10) != 0)
    {
        return false;
    }

    // Check .bin extension
    size_t len = strlen(filename);
    if (len < 20 || strcmp(filename + len - 4, ".bin") != 0)
    {
        return false;
    }

    // Try to extract variant (new format)
    char fileVariant[32] = {0};
    if (extractVariantFromFilename(filename, fileVariant, sizeof(fileVariant)))
    {
        // New format with variant - verify it matches current hardware
        if (strcmp(fileVariant, VARIANT_NAME) != 0)
        {
            logTimestamp();
            Serial.print("Firmware variant mismatch: file is for '");
            Serial.print(fileVariant);
            Serial.print("', you have '");
            Serial.print(VARIANT_NAME);
            Serial.println("'");
            return false;
        }

        // Variant matches
        return true;
    }

    // Couldn't extract variant - check if it's old format (spojboard-r{num}-{hash}.bin)
    // Old releases didn't have variant names, assume they're for matrixportal_s3 (original hardware)
    const char* afterPrefix = filename + 10; // Skip "spojboard-"
    if (afterPrefix[0] == 'r' && afterPrefix[1] >= '0' && afterPrefix[1] <= '9')
    {
        // Old format detected (starts with "r" immediately after "spojboard-")
        // Only accept for matrixportal_s3 (backward compatibility)
        if (strcmp(VARIANT_NAME, "matrixportal_s3") == 0)
        {
            logTimestamp();
            Serial.println("Old firmware format detected (no variant name). Assuming matrixportal_s3.");
            return true;
        }
        else
        {
            logTimestamp();
            Serial.println("Old firmware format (no variant) not compatible with this hardware.");
            return false;
        }
    }

    // Invalid format
    return false;
}

bool GitHubOTA::findBinaryAsset(JsonDocument& doc, char* outUrl, char* outName, size_t& outSize)
{
    // Find .bin asset matching current hardware variant
    JsonArray assets = doc["assets"];
    if (assets.isNull())
    {
        return false;
    }

    // Iterate through all assets to find matching variant
    for (JsonObject asset : assets)
    {
        const char* name = asset["name"];
        const char* url = asset["browser_download_url"];
        int size = asset["size"] | 0;

        if (name && url && size > 0)
        {
            // Check if it's a .bin file
            size_t nameLen = strlen(name);
            if (nameLen > 4 && strcmp(name + nameLen - 4, ".bin") == 0)
            {
                // Validate filename format and variant match
                if (validateFirmwareFilename(name))
                {
                    // Found matching firmware for this hardware
                    strlcpy(outName, name, 64);
                    strlcpy(outUrl, url, 256);
                    outSize = size;
                    return true;
                }
                // If validation fails, continue checking other .bin files
                // (they might be for different hardware variants)
            }
        }
    }

    // No matching firmware found for current hardware variant
    logTimestamp();
    Serial.print("No firmware found for hardware variant: ");
    Serial.println(VARIANT_NAME);
    return false;
}

GitHubOTA::ReleaseInfo GitHubOTA::checkForUpdate(const char* currentRelease)
{
    ReleaseInfo result = {};
    result.available = false;
    result.hasError = false;

    // Validate input
    if (!currentRelease || strlen(currentRelease) == 0)
    {
        setError(result, "Invalid current release");
        return result;
    }

    // Parse current release number
    int currentReleaseNum = atoi(currentRelease);
    if (currentReleaseNum <= 0)
    {
        setError(result, "Invalid current release number");
        return result;
    }

    logTimestamp();
    Serial.println("Checking for updates from GitHub...");

    // Make HTTP request to GitHub API
    HTTPClient http;
    http.begin(GITHUB_API_URL);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.addHeader("Accept", "application/vnd.github.v3+json");

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK)
    {
        logTimestamp();
        Serial.print("GitHub API Error: HTTP ");
        Serial.println(httpCode);

        char errorMsg[64];
        if (httpCode == 404)
        {
            snprintf(errorMsg, sizeof(errorMsg), "No releases found");
        }
        else if (httpCode == 403)
        {
            snprintf(errorMsg, sizeof(errorMsg), "GitHub API access denied");
        }
        else if (httpCode == 429)
        {
            snprintf(errorMsg, sizeof(errorMsg), "Rate limit exceeded, try later");
        }
        else
        {
            snprintf(errorMsg, sizeof(errorMsg), "GitHub API error: %d", httpCode);
        }

        setError(result, errorMsg);
        http.end();
        return result;
    }

    // Parse JSON response
    String payload = http.getString();
    http.end();

    DynamicJsonDocument doc(JSON_BUFFER_SIZE);
    DeserializationError error = deserializeJson(doc, payload);

    if (error)
    {
        logTimestamp();
        Serial.print("JSON Parse Error: ");
        Serial.println(error.c_str());
        setError(result, "Failed to parse GitHub response");
        return result;
    }

    // Extract tag name
    const char* tagName = doc["tag_name"];
    if (!tagName)
    {
        setError(result, "No tag_name in release");
        return result;
    }

    // Parse release number from tag
    int githubReleaseNum = parseReleaseNumber(tagName);
    if (githubReleaseNum < 0)
    {
        logTimestamp();
        Serial.print("Invalid tag format: ");
        Serial.println(tagName);
        setError(result, "Invalid release tag format");
        return result;
    }

    // Store tag name
    strlcpy(result.tagName, tagName, sizeof(result.tagName));
    result.releaseNumber = githubReleaseNum;

    // Extract release name
    const char* releaseName = doc["name"];
    if (releaseName)
    {
        strlcpy(result.releaseName, releaseName, sizeof(result.releaseName));
    }
    else
    {
        snprintf(result.releaseName, sizeof(result.releaseName), "Release %d", githubReleaseNum);
    }

    // Extract and truncate release notes
    const char* body = doc["body"];
    if (body)
    {
        strlcpy(result.releaseNotes, body, sizeof(result.releaseNotes));
    }
    else
    {
        strcpy(result.releaseNotes, "No release notes available.");
    }

    // Find .bin asset
    if (!findBinaryAsset(doc, result.assetUrl, result.assetName, result.assetSize))
    {
        setError(result, "No firmware file found in release");
        return result;
    }

    // Compare versions
    if (githubReleaseNum > currentReleaseNum)
    {
        result.available = true;
        logTimestamp();
        Serial.print("Update available: ");
        Serial.print(result.releaseName);
        Serial.print(" (");
        Serial.print(result.assetName);
        Serial.println(")");
    }
    else
    {
        logTimestamp();
        Serial.println("Already up to date");
    }

    return result;
}

bool GitHubOTA::downloadAndInstall(const char* assetUrl, size_t expectedSize, ProgressCallback onProgress)
{
    if (!assetUrl || strlen(assetUrl) == 0)
    {
        logTimestamp();
        Serial.println("Download Error: Invalid asset URL");
        return false;
    }

    // Extract filename from URL for variant validation
    const char* lastSlash = strrchr(assetUrl, '/');
    const char* filename = lastSlash ? lastSlash + 1 : assetUrl;

    // Validate hardware variant matches firmware
    char fileVariant[32] = {0};
    if (!extractVariantFromFilename(filename, fileVariant, sizeof(fileVariant)))
    {
        logTimestamp();
        Serial.println("Download Error: Invalid firmware filename format");
        return false;
    }

    if (strcmp(fileVariant, VARIANT_NAME) != 0)
    {
        logTimestamp();
        Serial.println("===========================================");
        Serial.println("DOWNLOAD BLOCKED: HARDWARE MISMATCH!");
        Serial.println("===========================================");
        Serial.print("Firmware file is for: ");
        Serial.println(fileVariant);
        Serial.print("Your hardware is: ");
        Serial.println(VARIANT_DISPLAY_NAME);
        Serial.println("");
        Serial.println("Flashing wrong firmware could damage hardware.");
        Serial.println("Download cancelled for safety.");
        Serial.println("===========================================");
        return false;
    }

    logTimestamp();
    Serial.print("Hardware variant verified: ");
    Serial.println(VARIANT_DISPLAY_NAME);
    Serial.print("Downloading firmware from: ");
    Serial.println(assetUrl);

    // Make HTTP request with redirect following enabled
    HTTPClient http;
    http.begin(assetUrl);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // Follow redirects automatically

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK)
    {
        logTimestamp();
        Serial.print("Download Error: HTTP ");
        Serial.println(httpCode);
        http.end();
        return false;
    }

    // Get content length
    int contentLength = http.getSize();
    if (contentLength <= 0)
    {
        logTimestamp();
        Serial.println("Download Error: Invalid content length");
        http.end();
        return false;
    }

    // Validate size matches expected
    if (expectedSize > 0 && (size_t)contentLength != expectedSize)
    {
        logTimestamp();
        Serial.print("Download Error: Size mismatch (expected ");
        Serial.print(expectedSize);
        Serial.print(", got ");
        Serial.print(contentLength);
        Serial.println(")");
        http.end();
        return false;
    }

    logTimestamp();
    Serial.print("Firmware size: ");
    Serial.print(contentLength);
    Serial.println(" bytes");

    // Get stream pointer
    WiFiClient* stream = http.getStreamPtr();
    if (!stream)
    {
        logTimestamp();
        Serial.println("Download Error: Failed to get stream");
        http.end();
        return false;
    }

    // Begin OTA update
    if (!Update.begin(contentLength))
    {
        logTimestamp();
        Serial.println("OTA Error: Failed to begin update");
        Update.printError(Serial);
        http.end();
        return false;
    }

    logTimestamp();
    Serial.println("Starting firmware download and flash...");

    // Stream download in chunks
    uint8_t buffer[1024];
    size_t written = 0;
    size_t lastProgressUpdate = 0;

    while (http.connected() && written < (size_t)contentLength)
    {
        size_t available = stream->available();
        if (available)
        {
            // Read chunk
            int bytesRead = stream->readBytes(buffer, min(sizeof(buffer), available));

            // Write to OTA partition
            if (Update.write(buffer, bytesRead) != (size_t)bytesRead)
            {
                logTimestamp();
                Serial.println("OTA Error: Failed to write data");
                Update.printError(Serial);
                http.end();
                return false;
            }

            written += bytesRead;

            // Call progress callback (every 10KB or 1%)
            if (onProgress)
            {
                size_t progressThreshold = contentLength / 100; // 1%
                if (progressThreshold < 10240)
                {
                    progressThreshold = 10240; // At least 10KB
                }

                if (written - lastProgressUpdate >= progressThreshold || written >= (size_t)contentLength)
                {
                    onProgress(written, contentLength);
                    lastProgressUpdate = written;
                }
            }
        }

        delay(1); // Yield to watchdog
    }

    http.end();

    // Verify download completed
    if (written != (size_t)contentLength)
    {
        logTimestamp();
        Serial.print("Download Error: Incomplete (");
        Serial.print(written);
        Serial.print("/");
        Serial.print(contentLength);
        Serial.println(" bytes)");
        return false;
    }

    // Finalize update with MD5 validation
    if (!Update.end(true))
    {
        logTimestamp();
        Serial.println("OTA Error: Update validation failed");
        Update.printError(Serial);
        return false;
    }

    logTimestamp();
    Serial.print("OTA Update Success: ");
    Serial.print(written);
    Serial.println(" bytes written and validated");

    return true;
}
