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

// Rest mode toggle JavaScript
const char SCRIPT_REST_MODE_TOGGLE[] PROGMEM = R"rawliteral(
<script>
async function toggleRestMode() {
    const btn = document.getElementById('restModeBtn');
    const isCurrentlyActive = (btn.innerText === 'Disable Rest Mode');
    const newState = !isCurrentlyActive;

    btn.disabled = true;
    btn.innerText = newState ? 'Disabling...' : 'Enabling...';

    try {
        const response = await fetch('/rest-mode', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ enabled: newState })
        });

        const data = await response.json();

        if (data.success) {
            // Reload page to refresh status
            window.location.reload();
        } else {
            alert('Failed to toggle rest mode: ' + (data.error || 'Unknown error'));
            btn.disabled = false;
            btn.innerText = isCurrentlyActive ? 'Disable Rest Mode' : 'Enable Rest Mode';
        }
    } catch (error) {
        alert('Error: ' + error.message);
        btn.disabled = false;
        btn.innerText = isCurrentlyActive ? 'Disable Rest Mode' : 'Enable Rest Mode';
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

function openPreview() {
    window.open('/preview', '_blank', 'width=800,height=600');
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

// Configuration form save with inline feedback
const char SCRIPT_CONFIG_SAVE[] PROGMEM = R"rawliteral(
<script>
document.addEventListener('DOMContentLoaded', function() {
    const form = document.getElementById('configForm');
    if (!form) return;

    form.addEventListener('submit', async function(e) {
        e.preventDefault();

        const submitBtn = form.querySelector('button[type="submit"]');
        const originalText = submitBtn.innerText;
        const originalBackground = submitBtn.style.background;

        // Disable button and show saving state
        submitBtn.disabled = true;
        submitBtn.innerText = '💾 Saving...';

        try {
            const formData = new FormData(form);
            const response = await fetch('/save', {
                method: 'POST',
                body: formData
            });

            // Handle error responses with details
            if (!response.ok) {
                const errorText = await response.text();
                throw new Error(errorText || 'HTTP ' + response.status);
            }

            const contentType = response.headers.get('content-type');

            // Check if response is JSON (normal save) or HTML (restart required)
            if (contentType && contentType.includes('application/json')) {
                // Normal save - show success feedback
                const data = await response.json();

                if (data.success) {
                    submitBtn.style.background = '#2ed573';
                    submitBtn.innerText = '✓ Saved!';

                    // Reset button after 2 seconds
                    setTimeout(() => {
                        submitBtn.style.background = originalBackground;
                        submitBtn.innerText = originalText;
                        submitBtn.disabled = false;
                    }, 2000);
                } else {
                    throw new Error(data.message || 'Save failed');
                }
            } else {
                // Restart required - submit form normally to navigate to restart page
                form.removeEventListener('submit', arguments.callee);
                form.submit();
            }
        } catch (error) {
            // Show error feedback with details
            submitBtn.style.background = '#ff6b6b';
            submitBtn.innerText = '✗ Save Failed';

            // Show error message below button
            let errorDiv = document.getElementById('save-error-msg');
            if (!errorDiv) {
                errorDiv = document.createElement('div');
                errorDiv.id = 'save-error-msg';
                errorDiv.style.cssText = 'margin-top:10px; padding:12px; background:#ff6b6b; color:#fff; border-radius:8px; font-size:14px;';
                submitBtn.parentElement.appendChild(errorDiv);
            }
            errorDiv.textContent = error.message;
            errorDiv.style.display = 'block';

            // Reset button after 5 seconds (longer for errors so user can read)
            setTimeout(() => {
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
