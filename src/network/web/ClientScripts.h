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
// City switching logic for new tab structure
document.addEventListener('DOMContentLoaded', function() {
    const citySelect = document.getElementById('city');
    if (!citySelect) return;

    // Add change listener to city selector
    citySelect.addEventListener('change', function() {
        const selectedCity = this.value;

        // Get section elements from Transit Data tab
        const pragueSection = document.getElementById('pragueSection');
        const berlinSection = document.getElementById('berlinSection');
        const mqttSection = document.getElementById('mqttSection');

        // Hide all sections first
        if (pragueSection) pragueSection.style.display = 'none';
        if (berlinSection) berlinSection.style.display = 'none';
        if (mqttSection) mqttSection.style.display = 'none';

        // Show selected section
        if (selectedCity === 'Prague' && pragueSection) {
            pragueSection.style.display = 'block';
        } else if (selectedCity === 'Berlin' && berlinSection) {
            berlinSection.style.display = 'block';
        } else if (selectedCity === 'MQTT' && mqttSection) {
            mqttSection.style.display = 'block';
        }

        // Show/hide refresh interval based on city
        const refreshDiv = document.getElementById('refreshInterval');
        if (refreshDiv && refreshDiv.parentElement) {
            // Refresh interval should be hidden for MQTT
            refreshDiv.parentElement.style.display = (selectedCity === 'MQTT') ? 'none' : 'block';
        }
    });
});
</script>
)rawliteral";


// Rest mode toggle JavaScript
const char SCRIPT_REST_MODE_TOGGLE[] PROGMEM = R"rawliteral(
<script>
async function toggleRestMode() {
    const btn = document.getElementById('restModeBtn');
    const isCurrentlyActive = btn.classList.contains('active');
    const newState = !isCurrentlyActive;

    btn.disabled = true;

    try {
        const response = await fetch('/rest-mode', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ enabled: newState })
        });

        const data = await response.json();

        if (data.success) {
            window.location.reload();
        } else {
            alert('Failed to toggle rest mode: ' + (data.error || 'Unknown error'));
            btn.disabled = false;
        }
    } catch (error) {
        alert('Error: ' + error.message);
        btn.disabled = false;
    }
}
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
        var dep = {
            line: formData.get('line' + i),
            destination: formData.get('dest' + i),
            eta: parseInt(formData.get('eta' + i)),
            platform: formData.get('platform' + i) || '',
            hasAC: formData.has('ac' + i)
        };
        var eta2 = formData.get('eta2_' + i);
        dep.secondEta = (eta2 !== null && eta2 !== '') ? parseInt(eta2) : -1;
        departures.push(dep);
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

// Tab navigation and switching
const char SCRIPT_TAB_NAVIGATION[] PROGMEM = R"rawliteral(
<script>
document.addEventListener('DOMContentLoaded', function() {
    const tabs = document.querySelectorAll('.tab');
    const tabContents = document.querySelectorAll('.tab-content');
    const formActions = document.querySelector('.form-actions');

    tabs.forEach(tab => {
        tab.addEventListener('click', function() {
            const targetTab = this.getAttribute('data-tab');

            tabs.forEach(t => t.classList.remove('active'));
            tabContents.forEach(content => content.classList.remove('active'));

            this.classList.add('active');
            document.getElementById('tab-' + targetTab).classList.add('active');

            // Hide Save button when System tab is active (no form fields to save)
            if (formActions) {
                formActions.style.display = (targetTab === 'system') ? 'none' : 'block';
            }
        });
    });
});
</script>
)rawliteral";

// Display tab handlers
const char SCRIPT_DISPLAY_TAB[] PROGMEM = R"rawliteral(
<script>
function addLineColorRow() {
    const tbody = document.getElementById('lineColorsTableBody');
    const emptyState = document.getElementById('emptyState');
    if (emptyState) { emptyState.remove(); }

    const row = tbody.insertRow();
    row.innerHTML = '<td><input type=\"text\" class=\"line-input\" placeholder=\"Line number\"></td><td><select class=\"color-select\"><option value=\"RED\">RED</option><option value=\"GREEN\">GREEN</option><option value=\"BLUE\">BLUE</option><option value=\"YELLOW\">YELLOW</option><option value=\"ORANGE\">ORANGE</option><option value=\"PURPLE\">PURPLE</option><option value=\"CYAN\">CYAN</option><option value=\"WHITE\">WHITE</option></select></td><td class=\"center\"><button type=\"button\" class=\"delete-btn\" onclick=\"deleteLineColorRow(this)\">Delete</button></td>';
}

function deleteLineColorRow(btn) {
    const row = btn.closest('tr');
    const tbody = document.getElementById('lineColorsTableBody');
    row.remove();

    if (tbody.rows.length === 0) {
        const emptyRow = tbody.insertRow();
        emptyRow.id = 'emptyState';
        const cell = emptyRow.insertCell(0);
        cell.colSpan = 3;
        cell.style.textAlign = 'center';
        cell.style.color = '#666';
        cell.style.padding = '20px';
        cell.textContent = 'No line colors configured. Click "+ Add Line Color" to add one.';
    }
}

function serializeLineColors() {
    const tbody = document.getElementById('lineColorsTableBody');
    if (!tbody) return '';

    const rows = tbody.querySelectorAll('tr:not(#emptyState)');
    const pairs = [];

    rows.forEach(row => {
        const lineInput = row.querySelector('.line-input');
        const colorSelect = row.querySelector('.color-select');

        if (lineInput && colorSelect && lineInput.value.trim()) {
            pairs.push(lineInput.value.trim() + '=' + colorSelect.value);
        }
    });

    const result = pairs.join(',');
    const hidden = document.getElementById('lineColorMapData');
    if (hidden) hidden.value = result;
    return result;
}
</script>
)rawliteral";

// Platform symbol configuration JavaScript
const char SCRIPT_PLATFORM_SYMBOLS[] PROGMEM = R"rawliteral(
<script>
function addPlatformSymbolRow() {
    var tbody = document.getElementById('platformSymbolsTableBody');
    var emptyState = document.getElementById('symbolEmptyState');
    if (emptyState) { emptyState.remove(); }

    var row = tbody.insertRow();
    var dirOptions = '<option value="1">1 - \u2191 N</option><option value="2">2 - \u2197 NE</option><option value="3">3 - \u2192 E</option><option value="4">4 - \u2198 SE</option><option value="5">5 - \u2193 S</option><option value="6">6 - \u2199 SW</option><option value="7">7 - \u2190 W</option><option value="8">8 - \u2196 NW</option>';
    // Safe: dirOptions is a static string with no user input; innerHTML is used for consistent structure with existing patterns (see addLineColorRow)
    row.innerHTML = '<td><input type="text" class="symbol-match" placeholder="B or ID:U693Z2P"></td><td><select class="symbol-dir">' + dirOptions + '</select></td><td class="center"><button type="button" class="delete-btn" onclick="deletePlatformSymbolRow(this)">Delete</button></td>';
}

function deletePlatformSymbolRow(btn) {
    var row = btn.closest('tr');
    var tbody = document.getElementById('platformSymbolsTableBody');
    row.remove();

    if (tbody.rows.length === 0) {
        var emptyRow = tbody.insertRow();
        emptyRow.id = 'symbolEmptyState';
        var cell = emptyRow.insertCell(0);
        cell.colSpan = 3;
        cell.style.textAlign = 'center';
        cell.style.color = '#666';
        cell.style.padding = '20px';
        cell.textContent = 'No platform symbols configured. Click "+ Add Symbol" to add one.';
    }
}

function serializePlatformSymbols() {
    var tbody = document.getElementById('platformSymbolsTableBody');
    if (!tbody) return '';

    var rows = tbody.querySelectorAll('tr:not(#symbolEmptyState)');
    var pairs = [];

    rows.forEach(function(row) {
        var matchInput = row.querySelector('.symbol-match');
        var dirSelect = row.querySelector('.symbol-dir');

        if (matchInput && dirSelect && matchInput.value.trim()) {
            pairs.push(matchInput.value.trim() + '=' + dirSelect.value);
        }
    });

    return pairs.join(',');
}
</script>
)rawliteral";

// Optional tab handlers
const char SCRIPT_OPTIONAL_TAB[] PROGMEM = R"rawliteral(
<script>
document.addEventListener('DOMContentLoaded', function() {
    const weatherCheckbox = document.getElementById('weatherEnabled');
    const weatherSettings = document.getElementById('weatherSettings');
    const weatherRefresh = document.getElementById('weatherRefresh');

    if (weatherCheckbox && weatherSettings && weatherRefresh) {
        weatherCheckbox.addEventListener('change', function() {
            const isEnabled = this.checked;
            weatherSettings.style.display = isEnabled ? 'grid' : 'none';
            weatherRefresh.style.display = isEnabled ? 'block' : 'none';
        });
    }
});
</script>
)rawliteral";

// System actions
const char SCRIPT_SYSTEM_ACTIONS[] PROGMEM = R"rawliteral(
<script>
function refreshDepartures() {
    fetch('/refresh', { method: 'POST' })
        .then(() => { alert('Refresh triggered. Departure data will update momentarily.'); })
        .catch(error => { alert('Error: ' + error.message); });
}

function rebootDevice() {
    if (confirm('Reboot the device? This will take about 10 seconds.')) {
        fetch('/reboot', { method: 'POST' })
            .then(() => {
                alert('Rebooting... This page will reload in 15 seconds.');
                setTimeout(() => window.location.reload(), 15000);
            })
            .catch(error => { alert('Error: ' + error.message); });
    }
}

function factoryReset() {
    if (confirm('⚠ WARNING: This will erase ALL settings and reboot into setup mode. Continue?')) {
        if (confirm('Are you absolutely sure? This cannot be undone!')) {
            fetch('/clear-config', { method: 'POST' })
                .then(() => {
                    alert('Settings erased. Device is rebooting into setup mode...');
                    setTimeout(() => window.location.href = '/', 20000);
                })
                .catch(error => { alert('Error: ' + error.message); });
        }
    }
}
</script>
)rawliteral";

// Configuration form save with per-tab field collection
const char SCRIPT_CONFIG_SAVE[] PROGMEM = R"rawliteral(
<script>
document.addEventListener('DOMContentLoaded', function() {
    var form = document.getElementById('configForm');
    if (!form) return;

    form.addEventListener('submit', async function(e) {
        e.preventDefault();

        var submitBtn = form.querySelector('button[type="submit"]');
        var originalText = submitBtn.innerText;
        var originalBackground = submitBtn.style.background;

        submitBtn.disabled = true;
        submitBtn.innerText = '💾 Saving...';

        try {
            // Determine active tab
            var activeTabBtn = document.querySelector('.tab.active');
            var activeTabName = activeTabBtn ? activeTabBtn.getAttribute('data-tab') : null;

            // AP mode: button text includes "Connect to WiFi"
            var isApMode = originalText.includes('Connect to WiFi');

            // Run serialization before collecting form data
            if ((activeTabName === 'display' || isApMode) && typeof serializeLineColors === 'function') {
                serializeLineColors();
            }
            if ((activeTabName === 'display' || isApMode) && typeof serializePlatformSymbols === 'function') {
                var symData = serializePlatformSymbols();
                var symInput = document.getElementById('platformSymbolMapData');
                if (symInput) symInput.value = symData;
            }

            // Validate rest mode periods (HH:MM-HH:MM format)
            if (activeTabName === 'optional' || isApMode) {
                var restInput = document.getElementById('restModePeriods');
                if (restInput && restInput.value.trim()) {
                    var periods = restInput.value.split(',');
                    for (var i = 0; i < periods.length; i++) {
                        var p = periods[i].trim();
                        if (!/^\d{2}:\d{2}-\d{2}:\d{2}$/.test(p)) {
                            throw new Error('Invalid rest period: "' + p + '". Use HH:MM-HH:MM format.');
                        }
                        var parts = p.split(/[-:]/);
                        var h1 = parseInt(parts[0]), m1 = parseInt(parts[1]);
                        var h2 = parseInt(parts[2]), m2 = parseInt(parts[3]);
                        if (h1 > 23 || m1 > 59 || h2 > 23 || m2 > 59) {
                            throw new Error('Invalid time in "' + p + '". Hours: 00-23, minutes: 00-59.');
                        }
                    }
                }
            }

            var formData;
            if (isApMode || !activeTabName || activeTabName === 'system') {
                // AP mode or fallback: send ALL fields
                formData = new FormData(form);
            } else {
                // STA mode: only collect inputs from the active tab panel
                formData = new FormData();
                formData.append('tab', activeTabName);
                var panel = document.getElementById('tab-' + activeTabName);
                if (panel) {
                    panel.querySelectorAll('input, select, textarea').forEach(function(el) {
                        if (!el.name) return;
                        if (el.type === 'checkbox') {
                            if (el.checked) formData.append(el.name, el.value || 'on');
                        } else if (el.type === 'radio') {
                            if (el.checked) formData.append(el.name, el.value);
                        } else {
                            formData.append(el.name, el.value);
                        }
                    });
                }
            }

            var response = await fetch('/save', {
                method: 'POST',
                body: formData
            });

            if (!response.ok) {
                var errorText = await response.text();
                throw new Error(errorText || 'HTTP ' + response.status);
            }

            var contentType = response.headers.get('content-type');

            if (contentType && contentType.includes('application/json')) {
                var data = await response.json();

                if (data.success) {
                    submitBtn.style.background = '#2ed573';
                    submitBtn.innerText = '✓ Saved!';

                    setTimeout(function() {
                        submitBtn.style.background = originalBackground;
                        submitBtn.innerText = originalText;
                        submitBtn.disabled = false;
                    }, 2000);
                } else {
                    throw new Error(data.message || 'Save failed');
                }
            } else {
                // Restart required - replace page with restart confirmation
                // Safe: HTML comes from our own trusted ESP32 server
                var restartHtml = await response.text();
                document.open();
                document.write(restartHtml);
                document.close();
            }
        } catch (error) {
            submitBtn.style.background = '#ff6b6b';
            submitBtn.innerText = '✗ Save Failed';

            var errorDiv = document.getElementById('save-error-msg');
            if (!errorDiv) {
                errorDiv = document.createElement('div');
                errorDiv.id = 'save-error-msg';
                errorDiv.style.cssText = 'margin-top:10px; padding:12px; background:#ff6b6b; color:#fff; border-radius:8px; font-size:14px;';
                submitBtn.parentElement.appendChild(errorDiv);
            }
            errorDiv.textContent = error.message;
            errorDiv.style.display = 'block';

            setTimeout(function() {
                submitBtn.style.background = originalBackground;
                submitBtn.innerText = originalText;
                submitBtn.disabled = false;
                if (errorDiv) errorDiv.style.display = 'none';
            }, 5000);
        }
    });
});
</script>
)rawliteral";

#endif // CLIENT_SCRIPTS_H
