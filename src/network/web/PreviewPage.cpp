#include "PreviewPage.h"
#include "WebTemplates.h"

String buildPreviewPage()
{
    String html = FPSTR(HTML_HEADER);
    html += "<h1>Display Preview</h1>";
    html += "<p style='text-align:center; color:#888; margin-top:-10px; margin-bottom:20px;'>Live view of LED matrix display</p>";

    // CSS Styles for LED Matrix Replica
    html += "<style>";
    html += ".led-display { ";
    html += "  background: #000; ";
    html += "  border: 3px solid #333; ";
    html += "  padding: 10px; ";
    html += "  margin: 20px auto; ";
    html += "  max-width: 800px; ";
    html += "  font-family: 'Courier New', monospace; ";
    html += "  box-shadow: 0 0 20px rgba(0,0,0,0.5); ";
    html += "}";
    html += ".led-row { ";
    html += "  display: flex; ";
    html += "  align-items: center; ";
    html += "  height: 40px; ";
    html += "  margin: 2px 0; ";
    html += "  padding: 4px 8px; ";
    html += "}";
    html += ".led-line-box { ";
    html += "  min-width: 45px; ";
    html += "  height: 28px; ";
    html += "  background: #000; ";
    html += "  border: 1px solid #333; ";
    html += "  display: flex; ";
    html += "  align-items: center; ";
    html += "  justify-content: center; ";
    html += "  font-size: 14px; ";
    html += "  font-weight: bold; ";
    html += "  margin-right: 8px; ";
    html += "  text-shadow: 0 0 3px currentColor; ";
    html += "}";
    html += ".led-destination { ";
    html += "  flex: 1; ";
    html += "  color: #fff; ";
    html += "  font-size: 14px; ";
    html += "  white-space: nowrap; ";
    html += "  overflow: hidden; ";
    html += "  text-overflow: ellipsis; ";
    html += "  text-shadow: 0 0 2px #fff; ";
    html += "}";
    html += ".led-eta { ";
    html += "  color: #fff; ";
    html += "  font-size: 14px; ";
    html += "  font-weight: bold; ";
    html += "  margin-left: auto; ";
    html += "  padding-left: 10px; ";
    html += "  text-shadow: 0 0 2px #fff; ";
    html += "}";
    html += ".led-status { ";
    html += "  display: flex; ";
    html += "  justify-content: space-between; ";
    html += "  color: #fff; ";
    html += "  font-size: 12px; ";
    html += "  padding: 8px; ";
    html += "  border-top: 1px solid #333; ";
    html += "  text-shadow: 0 0 2px #fff; ";
    html += "}";

    // Color classes for line boxes
    html += ".color-red { color: #ff0000; }";
    html += ".color-green { color: #00ff00; }";
    html += ".color-blue { color: #0000ff; }";
    html += ".color-yellow { color: #ffff00; }";
    html += ".color-orange { color: #ff8800; }";
    html += ".color-purple { color: #aa00ff; }";
    html += ".color-cyan { color: #00ffff; }";
    html += ".color-white { color: #ffffff; }";

    // ETA color classes
    html += ".eta-normal { color: #ffffff; }";
    html += ".eta-soon { color: #ffff00; }";
    html += ".eta-urgent { color: #ff0000; }";
    html += ".eta-delayed { color: #ff8800; }";

    // Weather colors
    html += ".weather-cold { color: #0088ff; }";
    html += ".weather-mild { color: #ffffff; }";
    html += ".weather-warm { color: #ffff00; }";
    html += ".weather-hot { color: #ff0000; }";

    // Status/error screen styles
    html += ".led-status-screen { ";
    html += "  display: flex; ";
    html += "  flex-direction: column; ";
    html += "  justify-content: center; ";
    html += "  align-items: center; ";
    html += "  min-height: 140px; ";
    html += "  padding: 20px; ";
    html += "  text-shadow: 0 0 3px currentColor; ";
    html += "}";
    html += ".led-status-line1 { ";
    html += "  font-size: 16px; ";
    html += "  font-weight: bold; ";
    html += "  margin-bottom: 10px; ";
    html += "}";
    html += ".led-status-line2 { ";
    html += "  font-size: 14px; ";
    html += "}";

    html += ".controls { text-align: center; margin: 20px; }";
    html += ".controls button { ";
    html += "  padding: 10px 20px; ";
    html += "  margin: 5px; ";
    html += "  font-size: 14px; ";
    html += "  cursor: pointer; ";
    html += "  background: #2ed573; ";
    html += "  color: #000; ";
    html += "  border: none; ";
    html += "  border-radius: 8px; ";
    html += "}";
    html += ".controls button:hover { background: #26de81; }";
    html += "#status { text-align: center; color: #888; margin: 10px; }";
    html += "</style>";

    // LED Display Container
    html += "<div class='led-display' id='ledDisplay'>";
    html += "<div id='departureRows'></div>";
    html += "<div class='led-status' id='statusBar'></div>";
    html += "</div>";

    // Controls
    html += "<div class='controls'>";
    html += "<button onclick='toggleAutoRefresh()' id='toggleBtn'>Pause</button>";
    html += "<button onclick='location.href=\"/\"'>Back to Dashboard</button>";
    html += "</div>";
    html += "<p id='status'>Loading...</p>";

    // JavaScript
    html += "<script>";

    // Color mapping function (matches DisplayColors.cpp logic)
    html += "function getLineColor(line) {";
    html += "  if (line === 'A') return 'green';";
    html += "  if (line === 'B') return 'yellow';";
    html += "  if (line === 'C') return 'red';";
    html += "  if (/^S\\d+$/.test(line)) return 'blue';";  // S-trains
    html += "  if (/^9[1-9]$/.test(line)) return 'cyan';";  // Night trams
    html += "  if (/^[1-2]\\d$/.test(line)) return 'white';";  // Trams
    html += "  if (/^(5[0-9]|[1-2]\\d\\d)$/.test(line)) return 'purple';";  // Buses
    html += "  if (/^9\\d\\d$/.test(line)) return 'cyan';";  // Night buses
    html += "  return 'yellow';";  // Default
    html += "}";

    // ETA color function
    html += "function getEtaColor(eta, isDelayed) {";
    html += "  if (isDelayed) return 'eta-delayed';";
    html += "  if (eta < 2) return 'eta-urgent';";
    html += "  if (eta <= 5) return 'eta-soon';";
    html += "  return 'eta-normal';";
    html += "}";

    // Weather color function
    html += "function getWeatherTempColor(temp) {";
    html += "  if (temp < 8) return 'weather-cold';";
    html += "  if (temp <= 16) return 'weather-mild';";
    html += "  if (temp <= 25) return 'weather-warm';";
    html += "  return 'weather-hot';";
    html += "}";

    // Weather icon mapping (using emoji approximations)
    html += "function getWeatherIcon(code) {";
    html += "  if (code === 0) return '\\u2600';";  // Clear sky - sun
    html += "  if (code <= 3) return '\\u2601';";  // Cloudy
    html += "  if (code <= 48) return '\\u2248';";  // Fog - approximately equal
    html += "  if (code <= 57) return '\\u2022';";  // Drizzle - bullet
    html += "  if (code <= 67) return '\\u2248';";  // Rain - approximately equal
    html += "  if (code <= 86) return '\\u2744';";  // Snow
    html += "  if (code >= 95) return '\\u26C8';";  // Thunderstorm
    html += "  return '\\u2601';";  // Default cloudy
    html += "}";

    // Update display function
    html += "async function updateDisplay() {";
    html += "  try {";
    html += "    const response = await fetch('/api/display-state');";
    html += "    const data = await response.json();";
    html += "    if (!data.success) throw new Error('API call failed');";
    html += "    const state = data.state;";

    // Decision tree matching DisplayManager::updateDisplay logic
    html += "    let contentHtml = '';";
    html += "    let statusHtml = '';";

    // Demo mode has highest priority
    html += "    if (state.demoModeActive) {";
    html += "      contentHtml = renderDepartures(data.departures);";
    html += "      statusHtml = renderStatusBar(data);";
    html += "    }";
    // AP Mode - Show credentials
    html += "    else if (state.apModeActive) {";
    html += "      contentHtml = renderStatusScreen('SpojBoard Setup', `WiFi: ${state.apSSID}<br>Password: ${state.apPassword}<br><br>Go to: 192.168.4.1`, '#00ffff');";
    html += "      statusHtml = '';";
    html += "    }";
    // WiFi connecting
    html += "    else if (!state.wifiConnected) {";
    html += "      contentHtml = renderStatusScreen('WiFi Connecting...', '', '#ffff00');";
    html += "      statusHtml = '';";
    html += "    }";
    // Setup required
    html += "    else if (!state.apiKeyConfigured) {";
    html += "      contentHtml = renderStatusScreen('Setup Required', 'http://' + window.location.hostname, '#00ffff');";
    html += "      statusHtml = '';";
    html += "    }";
    // API Error
    html += "    else if (state.apiError) {";
    html += "      contentHtml = renderStatusScreen('API Error', state.apiErrorMsg, '#ff0000');";
    html += "      statusHtml = renderStatusBar(data);";
    html += "    }";
    // No departures
    html += "    else if (state.departureCount === 0) {";
    html += "      const msg = state.stopName !== '' ? state.stopName : 'Waiting...';";
    html += "      contentHtml = renderStatusScreen('No Departures', msg, '#ffff00');";
    html += "      statusHtml = renderStatusBar(data);";
    html += "    }";
    // Rest mode
    html += "    else if (state.restModeActive) {";
    html += "      const msg = state.restModeManual ? 'Manual' : 'Scheduled';";
    html += "      contentHtml = renderStatusScreen('Rest Mode', msg, '#888888');";
    html += "      statusHtml = '';";
    html += "    }";
    // Normal departures
    html += "    else {";
    html += "      contentHtml = renderDepartures(data.departures);";
    html += "      statusHtml = renderStatusBar(data);";
    html += "    }";

    html += "    document.getElementById('departureRows').innerHTML = contentHtml;";
    html += "    document.getElementById('statusBar').innerHTML = statusHtml;";

    html += "    document.getElementById('status').textContent = 'Updated: ' + new Date().toLocaleTimeString();";
    html += "    document.getElementById('status').style.color = '#2ed573';";
    html += "  } catch (err) {";
    html += "    document.getElementById('status').textContent = 'Error: ' + err.message;";
    html += "    document.getElementById('status').style.color = '#ff6b6b';";
    html += "  }";
    html += "}";

    // Helper: Render departures
    html += "function renderDepartures(departures) {";
    html += "  return departures.map(dep => {";
    html += "    const color = getLineColor(dep.line);";
    html += "    const etaColor = getEtaColor(dep.eta, dep.isDelayed);";
    html += "    const etaText = dep.eta < 1 ? '<1' : dep.eta;";
    html += "    const acIndicator = dep.hasAC ? '*' : '';";
    html += "    const platformInfo = dep.platform && dep.platform !== '' ? ` [${dep.platform}]` : '';";
    html += "    return `<div class='led-row'>` +";
    html += "      `<div class='led-line-box color-${color}'>${dep.line}</div>` +";
    html += "      `<div class='led-destination'>${dep.destination}${acIndicator}${platformInfo}</div>` +";
    html += "      `<div class='led-eta ${etaColor}'>${etaText}\\'</div>` +";
    html += "    `</div>`;";
    html += "  }).join('');";
    html += "}";

    // Helper: Render status/error screen
    html += "function renderStatusScreen(line1, line2, color) {";
    html += "  return `<div class='led-status-screen' style='color:${color}'>` +";
    html += "    `<div class='led-status-line1'>${line1}</div>` +";
    html += "    (line2 ? `<div class='led-status-line2'>${line2}</div>` : '') +";
    html += "  `</div>`;";
    html += "}";

    // Helper: Render status bar
    html += "function renderStatusBar(data) {";
    html += "  let html = '<span>' + data.day + ' | ' + data.date + '</span>';";
    html += "  if (data.weather) {";
    html += "    const icon = getWeatherIcon(data.weather.code);";
    html += "    const tempColor = getWeatherTempColor(data.weather.temp);";
    html += "    html += '<span style=\"margin-left:10px;\">' + icon + ' <span class=\"' + tempColor + '\">' + data.weather.temp + '\u00B0</span> | ' + data.time + '</span>';";
    html += "  } else {";
    html += "    html += '<span style=\"margin-left:10px;\">| ' + data.time + '</span>';";
    html += "  }";
    html += "  return html;";
    html += "}";

    // Auto-refresh logic
    html += "let autoRefresh = true;";
    html += "let refreshInterval = null;";
    html += "function toggleAutoRefresh() {";
    html += "  autoRefresh = !autoRefresh;";
    html += "  const btn = document.getElementById('toggleBtn');";
    html += "  btn.textContent = autoRefresh ? 'Pause' : 'Resume';";
    html += "  if (autoRefresh) startAutoRefresh(); else stopAutoRefresh();";
    html += "}";
    html += "function startAutoRefresh() {";
    html += "  if (refreshInterval) clearInterval(refreshInterval);";
    html += "  refreshInterval = setInterval(updateDisplay, 3000);";
    html += "}";
    html += "function stopAutoRefresh() {";
    html += "  if (refreshInterval) clearInterval(refreshInterval);";
    html += "}";

    // Initialize
    html += "updateDisplay();";
    html += "startAutoRefresh();";
    html += "</script>";

    html += FPSTR(HTML_FOOTER);

    return html;
}
