#include "HttpUtils.h"
#include "Logger.h"
#include <WiFiClient.h>

void configureSecureClient(WiFiClientSecure& client)
{
    client.setInsecure(); // no cert validation (same as prior http.begin(httpsUrl))
}

String readHttpResponse(HTTPClient& http, size_t maxSize, bool debugMode)
{
    String payload;
    int contentLen = http.getSize();

    // Check if response uses chunked transfer encoding
    bool isChunked = (contentLen == -1);

    // Always log chunk detection (for debugging truncation issue)
    if (debugMode)
    {
        char msg[64];
        snprintf(msg, sizeof(msg), "HTTP: contentLen=%d, chunked=%d", contentLen, isChunked);
        logTimestamp();
        debugPrintln(msg);
    }

    if (!isChunked && contentLen > 0 && (size_t)contentLen < maxSize)
    {
        payload.reserve(contentLen + 1);
    }

    WiFiClient* stream = http.getStreamPtr();

    if (isChunked)
    {
        // Handle chunked transfer encoding
        char buffer[512];
        const int MAX_WAIT_MS = 5000; // Max wait time for next chunk
        int chunksRead = 0;

        while (payload.length() < maxSize)
        {
            // Wait for chunk size line to be available (with timeout)
            int waitCount = 0;
            while (!stream->available() && waitCount < MAX_WAIT_MS)
            {
                if (!http.connected())
                    break;
                delay(1);
                waitCount++;
            }

            if (!stream->available())
            {
                if (debugMode)
                {
                    char timeoutMsg[64];
                    snprintf(timeoutMsg, sizeof(timeoutMsg), "HTTP: Timeout after %d chunks, payload=%d", chunksRead, payload.length());
                    logTimestamp();
                    debugPrintln(timeoutMsg);
                }
                // Timeout waiting for next chunk - break if we got at least one chunk
                if (chunksRead > 0)
                    break;
                return payload; // No data received at all
            }

            // Read chunk size line (hex number followed by \r\n)
            String chunkSizeLine = stream->readStringUntil('\n');
            chunkSizeLine.trim();

            if (chunkSizeLine.length() == 0)
            {
                if (debugMode)
                {
                    logTimestamp();
                    debugPrintln("HTTP: Empty chunk size line");
                }
                break;
            }

            // Parse chunk size from hex
            long chunkSize = strtol(chunkSizeLine.c_str(), NULL, 16);
            if (chunkSize == 0)
            {
                if (debugMode)
                {
                    logTimestamp();
                    debugPrintln("HTTP: Got zero chunk (end)");
                }
                break; // Last chunk (normal termination)
            }

            chunksRead++;
            if (debugMode)
            {
                char chunkMsg[64];
                snprintf(chunkMsg, sizeof(chunkMsg), "HTTP: Chunk %d size=%ld (0x%s)", chunksRead, chunkSize, chunkSizeLine.c_str());
                logTimestamp();
                debugPrintln(chunkMsg);
            }

            // Read the chunk data
            int remaining = chunkSize;
            while (remaining > 0 && payload.length() < maxSize)
            {
                // Wait for data to be available (with timeout)
                waitCount = 0;
                while (!stream->available() && waitCount < MAX_WAIT_MS)
                {
                    if (!http.connected())
                        break;
                    delay(1);
                    waitCount++;
                }

                if (!stream->available())
                {
                    if (debugMode)
                    {
                        char msg[64];
                        snprintf(msg, sizeof(msg), "HTTP: Data timeout, remaining=%d", remaining);
                        logTimestamp();
                        debugPrintln(msg);
                    }
                    break;
                }

                int toRead = min(remaining, (int)sizeof(buffer) - 1);
                int bytesRead = stream->readBytes(buffer, toRead);
                if (bytesRead > 0)
                {
                    buffer[bytesRead] = '\0';
                    payload += buffer;
                    remaining -= bytesRead;
                }
                else
                {
                    if (debugMode)
                    {
                        logTimestamp();
                        debugPrintln("HTTP: readBytes returned 0");
                    }
                    break;
                }
            }

            // Skip any remaining data if we hit maxSize
            while (remaining > 0 && stream->available())
            {
                stream->read();
                remaining--;
            }

            // Read trailing \r\n after chunk data
            waitCount = 0;
            while (!stream->available() && waitCount < 100)
            {
                if (!http.connected())
                    break;
                delay(1);
                waitCount++;
            }
            if (stream->available())
            {
                stream->read(); // \r
            }
            if (stream->available())
            {
                stream->read(); // \n
            }
        }
    }
    else
    {
        // Non-chunked response - read based on Content-Length
        char buffer[512];
        int remaining = contentLen > 0 ? contentLen : -1;

        while ((http.connected() || stream->available()) && payload.length() < maxSize)
        {
            // Wait for data if needed
            int waitCount = 0;
            while (!stream->available() && http.connected() && waitCount < 5000)
            {
                delay(1);
                waitCount++;
            }

            if (!stream->available())
                break;

            int toRead = sizeof(buffer) - 1;
            if (remaining > 0 && remaining < toRead)
                toRead = remaining;

            int bytesRead = stream->readBytes(buffer, toRead);
            if (bytesRead > 0)
            {
                buffer[bytesRead] = '\0';
                payload += buffer;

                if (remaining > 0)
                    remaining -= bytesRead;

                // Stop if we've read all expected bytes
                if (remaining == 0)
                    break;
            }
            else
            {
                break;
            }
        }

        if (debugMode)
        {
            char finalMsg[64];
            snprintf(finalMsg, sizeof(finalMsg), "HTTP: Read %d/%d bytes", payload.length(), contentLen);
            logTimestamp();
            debugPrintln(finalMsg);
        }
    }

    return payload;
}
