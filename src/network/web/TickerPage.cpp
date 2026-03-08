#include "TickerPage.h"
#include "WebTemplates.h"

String buildTickerPage(bool tickerActive, const char* symbol, const char* interval,
                       int refreshInterval, bool apiKeySet)
{
    String html = FPSTR(HTML_HEADER);

    // Header
    html += "<div class='header'><div class='header-top'>";
    html += "<div class='header-title'><h1>SpojBoard</h1>";
    html += "<div class='header-subtitle'>Ticker Mode</div></div></div></div>";

    html += "<div class='content'>";

    // Info banner
    html += "<div class='banner banner-info' style='margin-bottom:24px;'>";
    html += "<span class='status-dot'></span>";
    html += "<div>Display a candlestick chart with real-time price data from Twelve Data API</div>";
    html += "</div>";

    // Active status banner
    if (tickerActive)
    {
        html += "<div class='banner banner-success' style='margin-bottom:24px;'>";
        html += "<span class='status-dot'></span>";
        html += "<div><strong>Ticker Mode Active</strong> - Displaying ";
        html += String(symbol);
        html += " candlestick chart</div>";
        html += "</div>";
    }

    // Ticker form
    html += "<form id='tickerForm' onsubmit='startTicker(event); return false;'>";

    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>Ticker Settings</div>";

    html += "<div class='grid'>";

    // Symbol
    html += "<div>";
    html += "<label for='tickerSymbol'>SYMBOL</label>";
    html += "<input type='text' id='tickerSymbol' name='ticker_symbol' value='";
    html += String(symbol);
    html += "' maxlength='15' required placeholder='e.g., BTC/USD, AAPL, ETH/USD'>";
    html += "<div class='help-text'>Twelve Data format (stocks: AAPL, crypto: BTC/USD)</div>";
    html += "</div>";

    // Interval
    html += "<div>";
    html += "<label for='tickerInterval'>CANDLE INTERVAL</label>";
    html += "<select id='tickerInterval' name='ticker_interval'>";
    html += "<option value='1h'";
    if (strcmp(interval, "1h") == 0) html += " selected";
    html += ">1 Hour</option>";
    html += "<option value='4h'";
    if (strcmp(interval, "4h") == 0) html += " selected";
    html += ">4 Hours</option>";
    html += "<option value='1day'";
    if (strcmp(interval, "1day") == 0) html += " selected";
    html += ">1 Day</option>";
    html += "</select>";
    html += "</div>";

    html += "</div>"; // End grid

    // API Key
    html += "<label for='tickerApiKey'>API KEY</label>";
    html += "<input type='password' id='tickerApiKey' name='ticker_api_key' value='' placeholder='";
    html += apiKeySet ? "****  (leave empty to keep current)" : "Enter your Twelve Data API key";
    html += "'>";
    html += "<div class='help-text'>Free tier: 800 calls/day. Get a key at <a href='https://twelvedata.com' style='color:#67e8f9;' target='_blank'>twelvedata.com</a></div>";

    // Refresh interval
    html += "<label for='tickerRefresh'>REFRESH INTERVAL (seconds)</label>";
    html += "<input type='number' id='tickerRefresh' name='ticker_refresh' value='";
    html += String(refreshInterval);
    html += "' min='120' max='600'>";
    html += "<div class='help-text'>Minimum 120s to stay within free tier limits</div>";

    html += "</div>"; // End form-group

    html += "<div class='form-actions'>";
    if (tickerActive)
    {
        html += "<button type='submit' class='btn-primary' style='background:#9b59b6;'>Update Ticker</button>";
    }
    else
    {
        html += "<button type='submit' class='btn-primary' style='background:#9b59b6;'>Start Ticker</button>";
    }
    html += "</div>";

    html += "</form>";

    // Stop button (when active)
    if (tickerActive)
    {
        html += "<div class='form-group'>";
        html += "<div class='form-group-title'>Ticker Status</div>";
        html += "<p style='color:#86efac; margin:0 0 16px;'>Ticker mode is active. The display shows a candlestick chart.</p>";
        html += "<form method='POST' action='/stop-ticker'>";
        html += "<button type='submit' class='danger'>Stop Ticker & Resume Departures</button>";
        html += "</form>";
        html += "</div>";
    }

    // Info card
    html += "<div class='card' style='background:#0a0a0a; border:1px solid #333;'>";
    html += "<h3 style='margin-top:0; font-size:14px; color:#999; text-transform:uppercase; letter-spacing:0.5px;'>About Ticker Mode</h3>";
    html += "<ul style='margin:8px 0; padding-left:20px; color:#999; font-size:13px; line-height:1.8;'>";
    html += "<li>Replaces departure display with a candlestick chart (rows 0-2)</li>";
    html += "<li>Status bar (date, time, weather) continues to display normally</li>";
    html += "<li>Ticker mode persists across reboots when enabled</li>";
    html += "<li>Rest mode overrides ticker (display off at night), ticker resumes when rest ends</li>";
    html += "<li>Supports stocks (AAPL, MSFT) and crypto (BTC/USD, ETH/USD)</li>";
    html += "</ul>";
    html += "</div>";

    html += "<div style='text-align:center; margin-top:24px;'>";
    html += "<a href='/' style='color:#67e8f9; text-decoration:none;'>&larr; Back to Dashboard</a>";
    html += "</div>";

    html += "</div>"; // End content

    // JavaScript
    html += "<script>";
    html += "function startTicker(e) {";
    html += "  e.preventDefault();";
    html += "  var symbol = document.getElementById('tickerSymbol').value;";
    html += "  var interval = document.getElementById('tickerInterval').value;";
    html += "  var apiKey = document.getElementById('tickerApiKey').value;";
    html += "  var refresh = document.getElementById('tickerRefresh').value;";
    html += "  var body = 'ticker_symbol=' + encodeURIComponent(symbol)";
    html += "    + '&ticker_interval=' + encodeURIComponent(interval)";
    html += "    + '&ticker_refresh=' + encodeURIComponent(refresh);";
    html += "  if (apiKey.length > 0) body += '&ticker_api_key=' + encodeURIComponent(apiKey);";
    html += "  fetch('/start-ticker', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:body})";
    html += "    .then(function(r){return r.json();})";
    html += "    .then(function(d){";
    html += "      if(d.success) window.location.href='/ticker';";
    html += "      else alert('Error: ' + (d.error||'Unknown'));";
    html += "    })";
    html += "    .catch(function(e){alert('Request failed: '+e);});";
    html += "}";
    html += "</script>";

    html += FPSTR(HTML_FOOTER);
    return html;
}
