#include "PreviewPage.h"
#include "WebTemplates.h"

String buildPreviewPage()
{
    String html = FPSTR(HTML_HEADER);

    // Header
    html += "<div class='header'><div class='header-top'>";
    html += "<div class='header-title'><h1>SpojBoard</h1>";
    html += "<div class='header-subtitle'>Live Display Preview</div></div>";

    // Action bar with controls
    html += "<div class='action-bar'>";
    html += "<button class='action-btn' onclick='toggleAutoRefresh()' id='toggleBtn' title='Pause/Resume'>⏸</button>";
    html += "<button class='action-btn' onclick='location.href=\"/\"' title='Back to Dashboard'>←</button>";
    html += "</div>";

    html += "</div></div>";

    html += "<div class='content'>";

    // Info banner
    html += "<div class='banner banner-info' style='margin-bottom:24px;'>";
    html += "<span class='status-dot'></span>";
    html += "<div>Real-time preview of what's currently displayed on the LED matrix. Updates every 3 seconds.</div>";
    html += "</div>";

    // CSS Styles for LED Matrix Replica
    html += "<style>";
    html += ".led-display { ";
    html += "  background: #000; ";
    html += "  border: 3px solid #333; ";
    html += "  padding: 15px; ";
    html += "  margin: 0 auto 24px; ";
    html += "  max-width: 900px; ";
    html += "  font-family: 'Courier New', monospace; ";
    html += "  box-shadow: 0 0 30px rgba(0,212,255,0.3), inset 0 0 20px rgba(0,0,0,0.8); ";
    html += "  border-radius: 8px; ";
    html += "}";
    html += ".led-row { ";
    html += "  display: flex; ";
    html += "  align-items: center; ";
    html += "  height: 45px; ";
    html += "  margin: 3px 0; ";
    html += "  padding: 6px 10px; ";
    html += "}";
    html += ".led-line-box { ";
    html += "  min-width: 50px; ";
    html += "  height: 32px; ";
    html += "  background: #0a0a0a; ";
    html += "  border: 2px solid #1a1a1a; ";
    html += "  display: flex; ";
    html += "  align-items: center; ";
    html += "  justify-content: center; ";
    html += "  font-size: 15px; ";
    html += "  font-weight: bold; ";
    html += "  margin-right: 12px; ";
    html += "  text-shadow: 0 0 5px currentColor; ";
    html += "  border-radius: 3px; ";
    html += "}";
    html += ".led-destination { ";
    html += "  flex: 1; ";
    html += "  color: #fff; ";
    html += "  font-size: 15px; ";
    html += "  white-space: nowrap; ";
    html += "  overflow: hidden; ";
    html += "  text-overflow: ellipsis; ";
    html += "  text-shadow: 0 0 3px #fff; ";
    html += "}";
    html += ".led-eta { ";
    html += "  color: #fff; ";
    html += "  font-size: 15px; ";
    html += "  font-weight: bold; ";
    html += "  margin-left: auto; ";
    html += "  padding-left: 12px; ";
    html += "  text-shadow: 0 0 3px #fff; ";
    html += "}";
    html += ".led-status { ";
    html += "  display: flex; ";
    html += "  justify-content: space-between; ";
    html += "  color: #fff; ";
    html += "  font-size: 13px; ";
    html += "  padding: 10px; ";
    html += "  margin-top: 5px; ";
    html += "  border-top: 2px solid #1a1a1a; ";
    html += "  text-shadow: 0 0 3px #fff; ";
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
    html += "  min-height: 160px; ";
    html += "  padding: 24px; ";
    html += "  text-shadow: 0 0 5px currentColor; ";
    html += "}";
    html += ".led-status-line1 { ";
    html += "  font-size: 18px; ";
    html += "  font-weight: bold; ";
    html += "  margin-bottom: 12px; ";
    html += "}";
    html += ".led-status-line2 { ";
    html += "  font-size: 15px; ";
    html += "}";
    html += "</style>";

    // LED Display Container
    html += "<div class='led-display' id='ledDisplay'>";
    html += "<div id='departureRows'></div>";
    html += "<div class='led-status' id='statusBar'></div>";
    html += "</div>";

    // Status card
    html += "<div class='card' style='background:#0a0a0a; border:1px solid #333;'>";
    html += "<div style='display:flex; align-items:center; justify-content:space-between;'>";
    html += "<div>";
    html += "<div style='font-size:12px; color:#999; text-transform:uppercase; letter-spacing:0.5px; margin-bottom:4px;'>Connection Status</div>";
    html += "<div id='status' style='font-size:14px; color:#2ed573;'>Loading...</div>";
    html += "</div>";
    html += "<div style='text-align:right;'>";
    html += "<div style='font-size:12px; color:#999; text-transform:uppercase; letter-spacing:0.5px; margin-bottom:4px;'>Auto Refresh</div>";
    html += "<div id='refreshStatus' style='font-size:14px; color:#67e8f9;'>Active (3s)</div>";
    html += "</div>";
    html += "</div>";
    html += "</div>";

    html += "</div>"; // End content

    // JavaScript
    html += "<script>";

    // Color mapping function (matches DisplayColors.cpp logic)
    html += "function getLineColor(line) {";
    html += "  if (line === 'A') return 'green';";
    html += "  if (line === 'B') return 'yellow';";
    html += "  if (line === 'C') return 'red';";
    html += "  if (/^S\\d+$/.test(line)) return 'blue';";
    html += "  if (/^9[1-9]$/.test(line)) return 'cyan';";
    html += "  if (/^[1-2]\\d$/.test(line)) return 'white';";
    html += "  if (/^(5[0-9]|[1-2]\\d\\d)$/.test(line)) return 'purple';";
    html += "  if (/^9\\d\\d$/.test(line)) return 'cyan';";
    html += "  return 'yellow';";
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

    // Weather icon mapping
    html += "function getWeatherIcon(code) {";
    html += "  if (code === 0) return '\\u2600';";
    html += "  if (code <= 3) return '\\u2601';";
    html += "  if (code <= 48) return '\\u2248';";
    html += "  if (code <= 57) return '\\u2022';";
    html += "  if (code <= 67) return '\\u2248';";
    html += "  if (code <= 86) return '\\u2744';";
    html += "  if (code >= 95) return '\\u26C8';";
    html += "  return '\\u2601';";
    html += "}";

    // Update display function
    html += "async function updateDisplay() {";
    html += "  try {";
    html += "    const response = await fetch('/api/display-state');";
    html += "    const data = await response.json();";
    html += "    if (!data.success) throw new Error('API call failed');";
    html += "    const state = data.state;";
    html += "    let contentHtml = '';";
    html += "    let statusHtml = '';";

    // Display logic (matching DisplayManager)
    html += "    if (state.demoModeActive) {";
    html += "      contentHtml = renderDepartures(data.departures);";
    html += "      statusHtml = renderStatusBar(data);";
    html += "    }";
    html += "    else if (state.apModeActive) {";
    html += "      contentHtml = renderStatusScreen('SpojBoard Setup', `WiFi: ${state.apSSID}<br>Password: ${state.apPassword}<br><br>Go to: 192.168.4.1`, '#00ffff');";
    html += "      statusHtml = '';";
    html += "    }";
    html += "    else if (!state.wifiConnected) {";
    html += "      contentHtml = renderStatusScreen('WiFi Connecting...', '', '#ffff00');";
    html += "      statusHtml = '';";
    html += "    }";
    html += "    else if (!state.apiKeyConfigured) {";
    html += "      contentHtml = renderStatusScreen('Setup Required', 'http://' + window.location.hostname, '#00ffff');";
    html += "      statusHtml = '';";
    html += "    }";
    html += "    else if (state.apiError) {";
    html += "      contentHtml = renderStatusScreen('API Error', state.apiErrorMsg, '#ff0000');";
    html += "      statusHtml = renderStatusBar(data);";
    html += "    }";
    html += "    else if (state.departureCount === 0) {";
    html += "      const msg = state.stopName !== '' ? state.stopName : 'Waiting...';";
    html += "      contentHtml = renderStatusScreen('No Departures', msg, '#ffff00');";
    html += "      statusHtml = renderStatusBar(data);";
    html += "    }";
    html += "    else if (state.restModeActive) {";
    html += "      const msg = state.restModeManual ? 'Manual' : 'Scheduled';";
    html += "      contentHtml = renderStatusScreen('Rest Mode', msg, '#888888');";
    html += "      statusHtml = '';";
    html += "    }";
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
    html += "    document.getElementById('status').style.color = '#fb7185';";
    html += "  }";
    html += "}";

    // Helper functions
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

    html += "function renderStatusScreen(line1, line2, color) {";
    html += "  return `<div class='led-status-screen' style='color:${color}'>` +";
    html += "    `<div class='led-status-line1'>${line1}</div>` +";
    html += "    (line2 ? `<div class='led-status-line2'>${line2}</div>` : '') +";
    html += "  `</div>`;";
    html += "}";

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

    // Auto-refresh controls
    html += "let autoRefresh = true;";
    html += "let refreshInterval = null;";
    html += "function toggleAutoRefresh() {";
    html += "  autoRefresh = !autoRefresh;";
    html += "  const btn = document.getElementById('toggleBtn');";
    html += "  const status = document.getElementById('refreshStatus');";
    html += "  if (autoRefresh) {";
    html += "    btn.innerHTML = '⏸';";
    html += "    btn.title = 'Pause';";
    html += "    status.textContent = 'Active (3s)';";
    html += "    status.style.color = '#67e8f9';";
    html += "    startAutoRefresh();";
    html += "  } else {";
    html += "    btn.innerHTML = '▶';";
    html += "    btn.title = 'Resume';";
    html += "    status.textContent = 'Paused';";
    html += "    status.style.color = '#999';";
    html += "    stopAutoRefresh();";
    html += "  }";
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
