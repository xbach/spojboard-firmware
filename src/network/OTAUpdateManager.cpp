#include "OTAUpdateManager.h"
#include "../utils/Logger.h"
#include <Update.h>
#include <Arduino.h>

OTAUpdateManager::OTAUpdateManager() : updating(false), totalSize(0), uploadedSize(0), progressCallback(nullptr)
{
    errorMsg[0] = '\0';
}

OTAUpdateManager::~OTAUpdateManager() {}

bool OTAUpdateManager::begin()
{
    logTimestamp();
    Serial.println("OTA Update Manager initialized");
    return true;
}

void OTAUpdateManager::handleUpload(WebServer* server, ProgressCallback onProgress)
{
    if (server == nullptr)
    {
        return;
    }

    progressCallback = onProgress;

    HTTPUpload& upload = server->upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        // Start of upload
        updating = true;
        uploadedSize = 0;
        totalSize = 0;
        errorMsg[0] = '\0';

        logTimestamp();
        Serial.print("OTA Update Start: ");
        Serial.println(upload.filename.c_str());

        // Begin OTA update
        // Use UPDATE_SIZE_UNKNOWN since we'll get size from Content-Length if available
        if (!Update.begin(UPDATE_SIZE_UNKNOWN))
        {
            setError("Failed to begin OTA update");
            Update.printError(Serial);
            updating = false;
            return;
        }

        // Try to get total size from Content-Length header
        if (server->hasHeader("Content-Length"))
        {
            totalSize = server->header("Content-Length").toInt();
            Serial.print("Total size: ");
            Serial.print(totalSize);
            Serial.println(" bytes");
        }
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        // Write chunk to flash
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
            setError("Failed to write OTA data");
            Update.printError(Serial);
            updating = false;
            return;
        }

        uploadedSize += upload.currentSize;

        // Call progress callback if provided
        if (progressCallback != nullptr && totalSize > 0)
        {
            progressCallback(uploadedSize, totalSize);
        }

        // Log progress every 10%
        static size_t lastLoggedPercent = 0;
        if (totalSize > 0)
        {
            size_t percent = (uploadedSize * 100) / totalSize;
            if (percent >= lastLoggedPercent + 10)
            {
                lastLoggedPercent = percent;
                Serial.print("Upload progress: ");
                Serial.print(percent);
                Serial.println("%");
            }
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        // End of upload
        updating = false;

        // Finalize OTA update with validation
        if (Update.end(true))
        {
            logTimestamp();
            Serial.print("OTA Update Success: ");
            Serial.print(uploadedSize);
            Serial.println(" bytes written");

            // Final progress callback
            if (progressCallback != nullptr)
            {
                progressCallback(uploadedSize, uploadedSize);
            }
        }
        else
        {
            setError("OTA update validation failed");
            Update.printError(Serial);
        }
    }
    else if (upload.status == UPLOAD_FILE_ABORTED)
    {
        // Upload was aborted
        updating = false;
        Update.abort();
        setError("Upload aborted");

        logTimestamp();
        Serial.println("OTA Update aborted");
    }
}

void OTAUpdateManager::setError(const char* msg)
{
    strlcpy(errorMsg, msg, sizeof(errorMsg));

    logTimestamp();
    Serial.print("OTA Error: ");
    Serial.println(errorMsg);
}
