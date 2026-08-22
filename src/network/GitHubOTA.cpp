#include "GitHubOTA.h"
#include "../config/AppConfig.h"
#include "../utils/Logger.h"
#include "../utils/HttpUtils.h"
#include "OtaAssetName.h"
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

    // New format: spojboard-<board>-[<display>-]r<n>-<id>.bin
    //
    // ANY geometry for THIS board is accepted here, deliberately. Installing a
    // different geometry build is a supported action -- it is how a user moves
    // a board to a different panel arrangement -- so the user's explicit choice
    // is not second-guessed at download time. Installing another BOARD's build
    // is never supported and is what this gate exists to stop.
    OtaAssetInfo info = otaClassifyAsset(filename, VARIANT_NAME);
    if (info.match == OtaAssetMatch::Bare || info.match == OtaAssetMatch::Display)
    {
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

int GitHubOTA::collectAssetOptions(JsonDocument& doc, AssetOption* out, int maxOptions)
{
    JsonArray assets = doc["assets"];
    if (assets.isNull())
    {
        return 0;
    }

    // TWO PASSES, most specific first, so options[0] is the best default and a
    // caller that ignores the rest still behaves sensibly. GitHub lists assets
    // in upload order, which is not something to depend on.
    //
    //   pass 0: spojboard-<board>-<display>-r<n>-<id>.bin   geometry builds
    //   pass 1: spojboard-<board>-r<n>-<id>.bin             bare build
    //
    // A release with no geometry builds yields one bare option and updates
    // every device. A release with geometry builds yields several and the user
    // chooses -- an r9 binary works at any geometry (panelRows is runtime
    // config), so there is nothing here that could pick correctly for them.
    int count = 0;
    for (int pass = 0; pass < 2 && count < maxOptions; pass++)
    {
        const OtaAssetMatch want = (pass == 0) ? OtaAssetMatch::Display : OtaAssetMatch::Bare;

        for (JsonObject asset : assets)
        {
            if (count >= maxOptions)
            {
                break;
            }

            const char* name = asset["name"];
            const char* url = asset["browser_download_url"];
            int size = asset["size"] | 0;

            if (!name || !url || size <= 0)
            {
                continue;
            }

            OtaAssetInfo info = otaClassifyAsset(name, VARIANT_NAME);
            if (info.match != want)
            {
                continue;
            }

            strlcpy(out[count].name, name, sizeof(out[count].name));
            strlcpy(out[count].url, url, sizeof(out[count].url));
            strlcpy(out[count].display, info.display, sizeof(out[count].display));
            out[count].size = (size_t)size;
            count++;
        }
    }

    logTimestamp();
    Serial.print("Assets for ");
    Serial.print(VARIANT_NAME);
    Serial.print(": ");
    Serial.println(count);

    return count;
}

void GitHubOTA::checkForUpdate(const char* currentRelease, ReleaseInfo& out)
{
    ReleaseInfo& result = out;
    result = ReleaseInfo{};
    result.available = false;
    result.hasError = false;

    // Validate input
    if (!currentRelease || strlen(currentRelease) == 0)
    {
        setError(result, "Invalid current release");
        return;
    }

    // Parse current release number
    int currentReleaseNum = atoi(currentRelease);
    if (currentReleaseNum <= 0)
    {
        setError(result, "Invalid current release number");
        return;
    }

    logTimestamp();
    Serial.println("Checking for updates from GitHub...");

    // Make HTTP request to GitHub API
    HTTPClient http;
    WiFiClientSecure client;
    configureSecureClient(client);
    http.begin(client, GITHUB_API_URL);
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
        return;
    }

    // Read with a cap (getString() had none) and parse through a filter that
    // keeps only the fields below. Without the filter the document grows with
    // every asset a release carries and silently starts returning NoMemory --
    // measured at four assets against the old 8KB buffer.
    String payload = readHttpResponse(http, JSON_READ_CAP);
    http.end();

    // Sized with margin and checked, because a filter document that overflows
    // does so SILENTLY: it simply drops its last keys, and the fields it drops
    // then read back as null from every asset. At 192 bytes this dropped
    // browser_download_url and size -- the two fields findBinaryAsset requires
    // -- which presents as "no firmware for this hardware", not as a parse
    // error. 256 is the measured requirement on a 64-bit host; slots are
    // smaller on the ESP32, so this is generous on purpose.
    StaticJsonDocument<512> filter;
    filter["tag_name"] = true;
    filter["name"] = true;
    filter["body"] = true;
    filter["assets"][0]["name"] = true;
    filter["assets"][0]["browser_download_url"] = true;
    filter["assets"][0]["size"] = true;

    if (filter.overflowed())
    {
        logTimestamp();
        Serial.println("FATAL: OTA filter document overflowed - update check cannot be trusted");
        setError(result, "Internal filter error");
        return;
    }

    DynamicJsonDocument doc(JSON_BUFFER_SIZE);
    DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

    if (error)
    {
        logTimestamp();
        Serial.print("JSON Parse Error: ");
        Serial.println(error.c_str());
        setError(result, "Failed to parse GitHub response");
        return;
    }

    // Extract tag name
    const char* tagName = doc["tag_name"];
    if (!tagName)
    {
        setError(result, "No tag_name in release");
        return;
    }

    // Parse release number from tag
    int githubReleaseNum = parseReleaseNumber(tagName);
    if (githubReleaseNum < 0)
    {
        logTimestamp();
        Serial.print("Invalid tag format: ");
        Serial.println(tagName);
        setError(result, "Invalid release tag format");
        return;
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
    result.optionCount = collectAssetOptions(doc, result.options, MAX_ASSET_OPTIONS);
    if (result.optionCount == 0)
    {
        setError(result, "No firmware file found in release");
        return;
    }

    // options[0] is the most specific build available; mirror it into the
    // single-asset fields so callers that do not offer a choice still work.
    strlcpy(result.assetUrl, result.options[0].url, sizeof(result.assetUrl));
    strlcpy(result.assetName, result.options[0].name, sizeof(result.assetName));
    result.assetSize = result.options[0].size;

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

    return;
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

    // Re-validate independently of the selection pass. This is the gate that
    // matters: it also covers a URL that did not come from collectAssetOptions.
    // It MUST use the same grammar as selection -- checking the raw variant
    // token against VARIANT_NAME here would reject every display-suffixed
    // asset the selector had just legitimately chosen.
    if (!validateFirmwareFilename(filename))
    {
        logTimestamp();
        Serial.println("===========================================");
        Serial.println("DOWNLOAD BLOCKED: HARDWARE MISMATCH!");
        Serial.println("===========================================");
        Serial.print("Firmware file: ");
        Serial.println(filename);
        Serial.print("Your hardware is: ");
        Serial.println(VARIANT_NAME);
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
    WiFiClientSecure client;
    configureSecureClient(client);
    http.begin(client, assetUrl);
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
