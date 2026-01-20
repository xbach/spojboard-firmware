#pragma once

#include <HTTPClient.h>
#include <WString.h>

// Read HTTP response using chunked reading (faster than getString's character-by-character)
// Set debugMode=true to enable detailed chunk reading logs
String readHttpResponse(HTTPClient& http, size_t maxSize, bool debugMode = false);
