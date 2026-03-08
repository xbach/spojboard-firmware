#ifndef TICKERAPI_H
#define TICKERAPI_H

#include <Arduino.h>

// ============================================================================
// Ticker Data Structures
// ============================================================================

struct TickerCandle
{
    float open;
    float high;
    float low;
    float close;
};

struct TickerData
{
    TickerCandle candles[30]; // 30 candles fit the display width
    int candleCount;
    float currentPrice;  // Latest close price
    float previousClose; // Previous candle's close (for trend arrow)
    char symbol[16];     // Displayed symbol (e.g., "BTC/USD")
    bool valid;          // false until first successful fetch
    bool hasError;       // API error flag
};

// ============================================================================
// TickerAPI Client (Twelve Data)
// ============================================================================

class TickerAPI
{
public:
    TickerAPI();

    /**
     * Fetch OHLC candle data from Twelve Data API
     * @param symbol Ticker symbol (e.g., "BTC/USD", "AAPL")
     * @param interval Candle interval ("1h", "4h", "1day")
     * @param apiKey Twelve Data API key
     * @return TickerData struct with candles and error status
     */
    TickerData fetchTicker(const char* symbol, const char* interval, const char* apiKey);

private:
    static constexpr int JSON_BUFFER_SIZE = 6144; // 6KB buffer for 30 candles
    static constexpr int HTTP_TIMEOUT_MS = 10000;  // 10 second timeout
    static constexpr int MAX_RETRIES = 3;
};

#endif // TICKERAPI_H
