#ifndef TICKER_PAGE_H
#define TICKER_PAGE_H

#include <Arduino.h>

// Build the ticker configuration HTML page
// @param tickerActive Whether ticker mode is currently active
// @param symbol Current ticker symbol
// @param interval Current candle interval
// @param refreshInterval Current refresh interval in seconds
// @param apiKeySet Whether an API key is configured
String buildTickerPage(bool tickerActive, const char* symbol, const char* interval,
                       int refreshInterval, bool apiKeySet);

#endif // TICKER_PAGE_H
