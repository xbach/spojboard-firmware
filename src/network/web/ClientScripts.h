#ifndef CLIENT_SCRIPTS_H
#define CLIENT_SCRIPTS_H

#include <Arduino.h>

// GitHub update check and download JavaScript
const char SCRIPT_GITHUB_UPDATE[] PROGMEM = R"rawliteral(
<script>
async function checkForUpdate(event) {
    event.preventDefault();
    const btn = document.getElementById('checkUpdateBtn');
    const status = document.getElementById('updateStatus');

    btn.disabled = true;
    btn.innerText = 'Checking...';
    status.style.display = 'none';

    try {
        const response = await fetch('/check-update');
        const data = await response.json();

        if (data.error) {
            throw new Error(data.error);
        }

        if (data.available) {
            status.innerHTML = `
                <div class='card' style='background: #2ed573; color: #000;'>
                    <h3 style='margin-top:0;'>Update Available!</h3>
                    <p><strong>Version:</strong> ${data.releaseName}</p>
                    <p><strong>File:</strong> ${data.fileName} (${formatBytes(data.fileSize)})</p>
                    <details style='margin: 10px 0;'>
                        <summary style='cursor:pointer; font-weight:bold;'>Release Notes</summary>
                        <div style='margin-top:10px; white-space:pre-wrap; font-size:0.9em;'>${escapeHtml(data.releaseNotes)}</div>
                    </details>
                    <button onclick="downloadUpdate('${data.assetUrl}', ${data.fileSize})"
                            style='background:#ff6b6b; color:#fff;'>
                        Download & Install
                    </button>
                </div>
            `;
        } else {
            status.innerHTML = `
                <div class='status ok'>You're up to date!</div>
            `;
        }
        status.style.display = 'block';
    } catch (error) {
        status.innerHTML = `
            <div class='status error'>Error: ${error.message}</div>
        `;
        status.style.display = 'block';
    } finally {
        btn.disabled = false;
        btn.innerText = 'Check for Updates';
    }
}

async function downloadUpdate(url, size) {
    if (!confirm('Download and install firmware? Device will reboot after installation.')) {
        return;
    }

    const status = document.getElementById('updateStatus');
    status.innerHTML = `
        <div class='card'>
            <h3>Downloading Firmware...</h3>
            <p style='color:#888; font-size:0.9em;'>Do not power off or disconnect!</p>
            <div style='background:#333; border-radius:5px; overflow:hidden; height:30px; margin:15px 0;'>
                <div id='downloadProgress' style='background:#00d4ff; height:100%; width:0%; transition:width 0.3s;'></div>
            </div>
            <p id='downloadText' style='text-align:center;'>Starting download...</p>
        </div>
    `;

    try {
        const response = await fetch('/download-update', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ assetUrl: url, expectedSize: size })
        });

        const data = await response.json();

        if (data.success) {
            status.innerHTML = `
                <div class='status ok'>
                    Update installed successfully! Device rebooting...
                    <p style='margin-top:20px;'>Please wait 15-20 seconds for device to restart.</p>
                </div>
            `;
            setTimeout(() => {
                status.innerHTML += `
                    <div style='margin-top:20px;'>
                        <button onclick='window.location.reload()' style='padding:12px 24px; font-size:16px; cursor:pointer; background:#2ed573; color:#000; border:none; border-radius:8px;'>Reconnect to Device</button>
                    </div>
                `;
            }, 15000);
        } else {
            status.innerHTML = `
                <div class='status error'>
                    Installation failed: ${data.error}
                </div>
            `;
        }
    } catch (error) {
        status.innerHTML = `
            <div class='status error'>
                Download failed: ${error.message}
            </div>
        `;
    }
}

function formatBytes(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / 1048576).toFixed(1) + ' MB';
}

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}
</script>
)rawliteral";

// City switching and ETA mode JavaScript
const char SCRIPT_CITY_SWITCH[] PROGMEM = R"rawliteral(
<script>
// Track the currently displayed city (starts with server-provided value)
let currentDisplayedCity = document.getElementById('citySelect').value;

// Update ETA mode help text based on selection
function updateEtaModeHelp() {
    const etaModeSelect = document.getElementById('mqttEtaModeInput');
    const helpText = document.getElementById('etaModeHelp');

    if (!etaModeSelect || !helpText) return;

    const isEtaMode = (etaModeSelect.value === '1');

    if (isEtaMode) {
        helpText.innerHTML = '<strong>Refresh Interval Recommendation:</strong> Set to <strong>10-60 seconds</strong> for frequent updates from server. Device displays pre-calculated ETAs without recalculation.';
    } else {
        helpText.innerHTML = '<strong>Refresh Interval Recommendation:</strong> Set to <strong>>60 seconds</strong> based on how many departures your server sends. Device recalculates ETAs every 10 seconds automatically.';
    }
}

// City switching logic - dynamically show/hide and update fields
function switchCity() {
    const newCity = document.getElementById('citySelect').value;
    const apiKeySection = document.getElementById('apiKeySection');
    const mqttSection = document.getElementById('mqttSection');
    const apiKeyInput = document.getElementById('apiKeyInput');
    const stopsInput = document.getElementById('stopsInput');
    const stopHelp = document.getElementById('stopHelp');
    const minDepTimeInput = document.getElementById('minDepTimeInput');
    const minDepTimeHelp = document.getElementById('minDepTimeHelp');

    // Save current visible stopIds to hidden field BEFORE switching
    // (currentDisplayedCity contains the city we're switching FROM)
    if (currentDisplayedCity === 'Prague') {
        document.getElementById('pragueStopsData').value = stopsInput.value;
    } else if (currentDisplayedCity === 'Berlin') {
        document.getElementById('berlinStopsData').value = stopsInput.value;
    }
    // MQTT doesn't use stops field

    // Now load the new city's data
    if (newCity === 'MQTT') {
        // Hide API key and stops for MQTT
        apiKeySection.style.display = 'none';
        mqttSection.style.display = 'block';
        stopsInput.style.display = 'none';
        stopHelp.style.display = 'none';

        // Remove required attribute for MQTT (doesn't use stops)
        stopsInput.removeAttribute('required');
        stopsInput.value = '';

        // Show minDepartureTime help text for MQTT
        if (minDepTimeHelp) minDepTimeHelp.style.display = 'block';

        // Update ETA mode help text
        updateEtaModeHelp();
    } else if (newCity === 'Prague') {
        // Show API key for Prague, hide MQTT
        apiKeySection.style.display = 'block';
        mqttSection.style.display = 'none';
        stopsInput.style.display = 'block';
        stopHelp.style.display = 'block';

        // Add required attribute back
        stopsInput.setAttribute('required', '');

        // Hide minDepartureTime help text (MQTT only)
        if (minDepTimeHelp) minDepTimeHelp.style.display = 'none';

        // Load Prague stops from hidden field
        stopsInput.value = document.getElementById('pragueStopsData').value;
        stopsInput.placeholder = 'e.g., U693Z2P';

        // Reset API key input field
        const pragueApiKey = document.getElementById('pragueApiKeyData').value;
        apiKeyInput.value = '';
        apiKeyInput.placeholder = pragueApiKey.length > 0 ? '****' : 'Enter API key';

        // Update help text
        stopHelp.innerHTML = 'Comma-separated PID stop IDs (e.g., U693Z2P). Find IDs at <a href="https://data.pid.cz/stops/json/stops.json" target="_blank">PID data</a>';
    } else {
        // Berlin: Hide API key and MQTT
        apiKeySection.style.display = 'none';
        mqttSection.style.display = 'none';
        stopsInput.style.display = 'block';
        stopHelp.style.display = 'block';

        // Add required attribute back
        stopsInput.setAttribute('required', '');

        // Hide minDepartureTime help text (MQTT only)
        if (minDepTimeHelp) minDepTimeHelp.style.display = 'none';

        // Load Berlin stops from hidden field
        stopsInput.value = document.getElementById('berlinStopsData').value;
        stopsInput.placeholder = 'e.g., 900013102';

        // Update help text
        stopHelp.innerHTML = 'Comma-separated numeric BVG stop IDs (e.g., 900013102). Find IDs at <a href="https://v6.bvg.transport.rest/" target="_blank">BVG API</a>';
    }

    // Update the tracked city to the new one
    currentDisplayedCity = newCity;
}

// Initialize city-specific display on page load
document.addEventListener('DOMContentLoaded', function() {
    switchCity();
    updateEtaModeHelp();
});
</script>
)rawliteral";

// Line color configuration JavaScript
const char SCRIPT_LINE_COLORS[] PROGMEM = R"rawliteral(
<script>
// Add new empty row to line color table
function addLineRow() {
    const tbody = document.getElementById('lineColorRows');
    const row = tbody.insertRow();

    // Line input cell
    const cell1 = row.insertCell(0);
    cell1.style.padding = '8px';
    cell1.innerHTML = "<input type='text' class='lineInput' style='width:80px; padding:5px;' maxlength='5' placeholder='A or 9*'>";

    // Color select cell
    const cell2 = row.insertCell(1);
    cell2.style.padding = '8px';
    const colors = ['RED', 'GREEN', 'BLUE', 'YELLOW', 'ORANGE', 'PURPLE', 'CYAN', 'WHITE'];
    let selectHtml = "<select class='colorSelect' style='width:100%; padding:5px;'>";
    colors.forEach(color => {
        selectHtml += `<option value='${color}'>${color}</option>`;
    });
    selectHtml += "</select>";
    cell2.innerHTML = selectHtml;

    // Delete button cell
    const cell3 = row.insertCell(2);
    cell3.style.padding = '8px';
    cell3.style.textAlign = 'center';
    cell3.innerHTML = "<button type='button' onclick='deleteLineRow(this)' style='background:#ff6b6b; color:#fff; padding:5px 10px; border:none; cursor:pointer;'>X</button>";
}

// Delete row from table
function deleteLineRow(btn) {
    const row = btn.closest('tr');
    row.remove();
}

// Serialize table to hidden input before form submit
function serializeLineColors() {
    const rows = document.querySelectorAll('#lineColorRows tr');
    const mappings = [];

    rows.forEach(row => {
        const lineInput = row.querySelector('.lineInput');
        const colorSelect = row.querySelector('.colorSelect');

        if (lineInput && colorSelect) {
            const line = lineInput.value.trim().toUpperCase();
            const color = colorSelect.value;

            // Only include non-empty line names
            if (line.length > 0) {
                mappings.push(`${line}=${color}`);
            }
        }
    });

    // Store as comma-separated string
    document.getElementById('lineColorMapData').value = mappings.join(',');

    return true;  // Allow form submission
}

// Attach serializer to form submit
document.querySelector('form').addEventListener('submit', function(e) {
    serializeLineColors();
});
</script>
)rawliteral";

// Rest mode configuration JavaScript
const char SCRIPT_REST_MODE[] PROGMEM = R"rawliteral(
<script>
// Add new empty row to rest mode table
function addRestRow() {
    const tbody = document.getElementById('restModeRows');
    const row = tbody.insertRow();

    // From Time: Hour + Minute dropdowns
    const cell1 = row.insertCell(0);
    cell1.style.padding = '8px';
    let fromHtml = '<select class="restFromHour" style="padding:5px; margin-right:5px;">';
    for (let h = 0; h < 24; h++) {
        const hour = String(h).padStart(2, '0');
        fromHtml += `<option value="${hour}">${hour}</option>`;
    }
    fromHtml += '</select>:<select class="restFromMin" style="padding:5px;">';
    fromHtml += '<option value="00">00</option><option value="30">30</option>';
    fromHtml += '</select>';
    cell1.innerHTML = fromHtml;

    // To Time: Hour + Minute dropdowns
    const cell2 = row.insertCell(1);
    cell2.style.padding = '8px';
    let toHtml = '<select class="restToHour" style="padding:5px; margin-right:5px;">';
    for (let h = 0; h < 24; h++) {
        const hour = String(h).padStart(2, '0');
        toHtml += `<option value="${hour}">${hour}</option>`;
    }
    toHtml += '</select>:<select class="restToMin" style="padding:5px;">';
    toHtml += '<option value="00">00</option><option value="30">30</option>';
    toHtml += '</select>';
    cell2.innerHTML = toHtml;

    // Delete button
    const cell3 = row.insertCell(2);
    cell3.style.padding = '8px';
    cell3.style.textAlign = 'center';
    cell3.innerHTML = "<button type='button' onclick='deleteRestRow(this)' style='background:#ff6b6b; color:#fff; padding:5px 10px; border:none; cursor:pointer;'>X</button>";
}

// Delete row from table
function deleteRestRow(btn) {
    btn.closest('tr').remove();
}

// Serialize table to hidden input before form submit
function serializeRestPeriods() {
    const rows = document.querySelectorAll('#restModeRows tr');
    const periods = [];

    rows.forEach(row => {
        const fromHour = row.querySelector('.restFromHour').value;
        const fromMin = row.querySelector('.restFromMin').value;
        const toHour = row.querySelector('.restToHour').value;
        const toMin = row.querySelector('.restToMin').value;

        const fromTime = `${fromHour}:${fromMin}`;
        const toTime = `${toHour}:${toMin}`;

        periods.push(`${fromTime}-${toTime}`);
    });

    document.getElementById('restPeriodsData').value = periods.join(',');
    return true;
}

// Attach serializer to form submit (must happen after DOM ready)
document.addEventListener('DOMContentLoaded', function() {
    const form = document.querySelector('form');
    const existingHandler = form.onsubmit;

    form.addEventListener('submit', function(e) {
        // Call existing handlers (e.g., serializeLineColors)
        if (existingHandler) {
            existingHandler(e);
        }
        serializeRestPeriods();
    });
});
</script>
)rawliteral";

// Demo page JavaScript
const char SCRIPT_DEMO[] PROGMEM = R"rawliteral(
<script>
async function startDemo(event) {
    event.preventDefault();
    const form = document.getElementById('demoForm');
    const formData = new FormData(form);

    // Build JSON payload
    const departures = [];
    for (let i = 1; i <= 3; i++) {
        departures.push({
            line: formData.get('line' + i),
            destination: formData.get('dest' + i),
            eta: parseInt(formData.get('eta' + i)),
            platform: formData.get('platform' + i) || '',
            hasAC: formData.has('ac' + i)
        });
    }

    try {
        const response = await fetch('/start-demo', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ departures: departures })
        });

        const data = await response.json();

        if (data.success) {
            document.getElementById('demoStatus').innerHTML = `
                <div class='status ok'>
                    Demo mode active! Check your LED display.
                    <p style='margin-top:10px; color:#000;'>The display is now showing your sample departure data. API polling and time updates are paused.</p>
                </div>
            `;
            document.getElementById('stopDemoForm').style.display = 'block';
        } else {
            document.getElementById('demoStatus').innerHTML = `
                <div class='status error'>Failed to start demo: ${data.error}</div>
            `;
        }
    } catch (error) {
        document.getElementById('demoStatus').innerHTML = `
            <div class='status error'>Error: ${error.message}</div>
        `;
    }
}
</script>
)rawliteral";

// OTA upload progress JavaScript
const char SCRIPT_OTA_UPLOAD[] PROGMEM = R"rawliteral(
<script>
document.getElementById('uploadForm').onsubmit = function() {
    document.getElementById('uploadBtn').disabled = true;
    document.getElementById('progress').style.display = 'block';
    document.getElementById('progressText').innerText = 'Uploading firmware...';
};
</script>
)rawliteral";

#endif // CLIENT_SCRIPTS_H
