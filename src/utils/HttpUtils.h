#pragma once

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WString.h>

// Configure a WiFiClientSecure for our APIs: no cert validation, guaranteeing the
// prior behavior of http.begin(httpsUrl) regardless of the framework's implicit
// default. Pass the client to http.begin(client, url); the client must outlive the
// request/retry cycle.
//
// Note: arduino-esp32 3.x (NetworkClientSecure) has no setBufferSizes() — the
// mbedTLS record buffer (~16KB) is fixed in the precompiled framework. Internal-RAM
// relief for multi-panel + HTTPS therefore comes from the display side (reduced
// HUB75 color depth), not from shrinking TLS buffers here.
void configureSecureClient(WiFiClientSecure& client);

// Read HTTP response using chunked reading (faster than getString's character-by-character)
// Set debugMode=true to enable detailed chunk reading logs
String readHttpResponse(HTTPClient& http, size_t maxSize, bool debugMode = false);
