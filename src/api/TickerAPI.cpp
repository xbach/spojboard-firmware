#include "TickerAPI.h"
#include "../utils/Logger.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

TickerAPI::TickerAPI()
{
}

TickerData TickerAPI::fetchTicker(const char* symbol, const char* interval, const char* apiKey)
{
    TickerData result = {};
    result.candleCount = 0;
    result.currentPrice = 0;
    result.previousClose = 0;
    result.valid = false;
    result.hasError = false;
    strlcpy(result.symbol, symbol, sizeof(result.symbol));

    logTimestamp();
    debugPrintln("Ticker: Starting fetch...");
    logMemory("ticker_start");

    // Validate inputs
    if (!apiKey || apiKey[0] == '\0')
    {
        result.hasError = true;
        logTimestamp();
        debugPrintln("Ticker: No API key configured");
        return result;
    }

    if (!symbol || symbol[0] == '\0')
    {
        result.hasError = true;
        logTimestamp();
        debugPrintln("Ticker: No symbol configured");
        return result;
    }

    // Build API URL
    char url[256];
    snprintf(url, sizeof(url),
             "https://api.twelvedata.com/time_series?symbol=%s&interval=%s&outputsize=30&apikey=%s&format=JSON",
             symbol, interval, apiKey);

    logTimestamp();
    debugPrint("Ticker: Fetching ");
    debugPrint(symbol);
    debugPrint(" @ ");
    debugPrintln(interval);

    HTTPClient http;
    int httpCode = -1;
    bool success = false;

    // Retry logic with exponential backoff
    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++)
    {
        if (attempt > 1)
        {
            int delayMs = 2000 * (1 << (attempt - 2)); // 2s, 4s
            logTimestamp();
            debugPrint("Ticker: Retry ");
            debugPrint(String(attempt - 1).c_str());
            debugPrint("/");
            debugPrintln(String(MAX_RETRIES - 1).c_str());
            delay(delayMs);
        }

        http.begin(url);
        http.setTimeout(HTTP_TIMEOUT_MS);

        httpCode = http.GET();

        logTimestamp();
        debugPrint("Ticker: HTTP code: ");
        debugPrintln(String(httpCode).c_str());

        if (httpCode == HTTP_CODE_OK)
        {
            success = true;
            break;
        }

        // Don't retry on 4xx client errors
        if (httpCode >= 400 && httpCode < 500)
        {
            break;
        }
    }

    if (!success || httpCode != HTTP_CODE_OK)
    {
        result.hasError = true;
        http.end();

        if (httpCode == 429)
        {
            logTimestamp();
            debugPrintln("Ticker: Rate limited (429) - extend backoff");
        }
        else
        {
            logTimestamp();
            debugPrint("Ticker: Fetch failed with HTTP code: ");
            debugPrintln(String(httpCode).c_str());
        }
        return result;
    }

    // Parse JSON response
    String payload = http.getString();
    http.end();

    logTimestamp();
    debugPrint("Ticker: Response size: ");
    debugPrint(String(payload.length()).c_str());
    debugPrintln(" bytes");

    DynamicJsonDocument doc(JSON_BUFFER_SIZE);
    DeserializationError error = deserializeJson(doc, payload);

    if (error)
    {
        result.hasError = true;
        logTimestamp();
        debugPrint("Ticker: JSON parse error: ");
        debugPrintln(error.c_str());
        return result;
    }

    // Check for API error response (e.g., invalid API key)
    if (doc.containsKey("status") && doc["status"] == "error")
    {
        result.hasError = true;
        logTimestamp();
        debugPrint("Ticker: API error: ");
        const char* msg = doc["message"] | "Unknown error";
        debugPrintln(msg);
        return result;
    }

    // Extract candle data from "values" array
    if (!doc.containsKey("values"))
    {
        result.hasError = true;
        logTimestamp();
        debugPrintln("Ticker: Missing 'values' in response");
        return result;
    }

    JsonArray values = doc["values"];
    int count = values.size();
    if (count == 0)
    {
        result.hasError = true;
        logTimestamp();
        debugPrintln("Ticker: Empty values array");
        return result;
    }

    // Values are newest-first — reverse into candles[] so [0] = oldest (leftmost)
    int maxCandles = (count > 30) ? 30 : count;
    result.candleCount = maxCandles;

    for (int i = 0; i < maxCandles; i++)
    {
        int srcIdx = maxCandles - 1 - i; // Reverse order
        JsonObject candle = values[srcIdx];

        result.candles[i].open = atof(candle["open"] | "0");
        result.candles[i].high = atof(candle["high"] | "0");
        result.candles[i].low = atof(candle["low"] | "0");
        result.candles[i].close = atof(candle["close"] | "0");
    }

    // Set current and previous prices
    if (maxCandles >= 2)
    {
        result.currentPrice = result.candles[maxCandles - 1].close;
        result.previousClose = result.candles[maxCandles - 2].close;
    }
    else if (maxCandles == 1)
    {
        result.currentPrice = result.candles[0].close;
        result.previousClose = result.candles[0].open;
    }

    result.valid = true;

    logTimestamp();
    char msg[64];
    snprintf(msg, sizeof(msg), "Ticker: Success - %d candles, price=%.2f", maxCandles, result.currentPrice);
    debugPrintln(msg);
    logMemory("ticker_complete");

    return result;
}
