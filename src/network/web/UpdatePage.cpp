#include "UpdatePage.h"
#include "WebTemplates.h"
#include "ClientScripts.h"
#include "../../config/AppConfig.h"

String buildUpdatePage()
{
    String html = FPSTR(HTML_HEADER);

    // Header
    html += "<div class='header'><div class='header-top'>";
    html += "<div class='header-title'><h1>SpojBoard</h1>";
    html += "<div class='header-subtitle'>Firmware Update</div></div></div></div>";

    html += "<div class='content'>";

    // Warning banner
    html += "<div class='banner banner-warning' style='margin-bottom:24px;'>";
    html += "<span class='status-dot'></span>";
    html += "<div><strong>⚠️ Critical Safety Instructions</strong>";
    html += "<ul style='margin:8px 0 0 20px; padding:0; font-size:13px; line-height:1.8;'>";
    html += "<li>Do NOT power off or disconnect during update</li>";
    html += "<li>Update takes 1-2 minutes to complete</li>";
    html += "<li>Device will reboot automatically after update</li>";
    html += "<li>Only flash firmware for <strong>" + String(VARIANT_DISPLAY_NAME) + "</strong></li>";
    html += "</ul></div>";
    html += "</div>";

    // Current firmware info card
    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>Current Firmware</div>";

    char currentBuildId[10];
    snprintf(currentBuildId, sizeof(currentBuildId), "%08x", BUILD_ID);

    html += "<div class='info-row'>";
    html += "<span class='info-label'>Hardware:</span>";
    html += "<span class='info-value'>" + String(VARIANT_DISPLAY_NAME) + "</span>";
    html += "</div>";

    html += "<div class='info-row'>";
    html += "<span class='info-label'>Release:</span>";
    html += "<span class='info-value'>" + String(FIRMWARE_RELEASE) + "</span>";
    html += "</div>";

    html += "<div class='info-row'>";
    html += "<span class='info-label'>Build ID:</span>";
    html += "<span class='info-value'>" + String(currentBuildId) + "</span>";
    html += "</div>";

    html += "</div>"; // End form-group

    // Upload form card
    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>Upload New Firmware</div>";

    html += "<form method='POST' action='/update' enctype='multipart/form-data' id='uploadForm'>";
    html += "<label for='firmware'>FIRMWARE FILE (.bin)</label>";
    html += "<input type='file' id='firmware' name='firmware' accept='.bin' required style='margin-bottom:15px;'>";
    html += "<div class='help-text'>Select the firmware .bin file for your hardware variant</div>";
    html += "<button type='submit' class='btn-primary' id='uploadBtn'>📤 Upload Firmware</button>";
    html += "</form>";

    // Progress indicator
    html += "<div id='progress' style='display:none; margin-top:24px;'>";
    html += "<div style='background:#1a1a1a; height:6px; border-radius:3px; overflow:hidden;'>";
    html += "<div id='progressBar' style='background:linear-gradient(90deg, #67e8f9, #2ed573); height:100%; width:0%; transition:width 0.3s;'></div>";
    html += "</div>";
    html += "<p id='progressText' style='text-align:center; margin-top:10px; color:#67e8f9; font-size:14px;'>Uploading...</p>";
    html += "</div>";

    html += "</div>"; // End form-group

    html += "<div style='text-align:center; margin-top:24px;'>";
    html += "<a href='/' style='color:#67e8f9; text-decoration:none;'>← Back to Dashboard</a>";
    html += "</div>";

    html += "</div>"; // End content

    // JavaScript for progress
    html += FPSTR(SCRIPT_OTA_UPLOAD);

    html += FPSTR(HTML_FOOTER);
    return html;
}

String buildUpdateBlockedPage()
{
    String html = FPSTR(HTML_HEADER);

    // Header
    html += "<div class='header'><div class='header-top'>";
    html += "<div class='header-title'><h1>SpojBoard</h1>";
    html += "<div class='header-subtitle'>Firmware Update</div></div></div></div>";

    html += "<div class='content'>";

    // Error banner
    html += "<div class='banner banner-error' style='margin-bottom:24px;'>";
    html += "<span class='status-dot'></span>";
    html += "<div><strong>🚫 Update Unavailable</strong></div>";
    html += "</div>";

    // Explanation card
    html += "<div class='card'>";
    html += "<h2 style='margin-top:0; color:#fb7185; font-size:18px;'>Security Restriction</h2>";
    html += "<p style='margin:12px 0; color:#f5f5f5; font-size:14px;'>Firmware updates are disabled in AP (setup) mode for security reasons.</p>";
    html += "<p style='color:#999; font-size:13px;'>Please connect the device to your WiFi network first, then access the update page from STA mode.</p>";
    html += "</div>";

    // Instructions card
    html += "<div class='card' style='background:#0a0a0a; border:1px solid #333;'>";
    html += "<h3 style='margin-top:0; font-size:14px; color:#999; text-transform:uppercase; letter-spacing:0.5px;'>How to Enable Updates</h3>";
    html += "<ol style='margin:8px 0; padding-left:20px; color:#999; font-size:13px; line-height:1.8;'>";
    html += "<li>Go to the <strong style='color:#67e8f9;'>Dashboard</strong></li>";
    html += "<li>Configure your <strong style='color:#67e8f9;'>WiFi credentials</strong></li>";
    html += "<li>Save and connect to your network</li>";
    html += "<li>Access the device via its IP address</li>";
    html += "<li>Navigate to System tab → Firmware Updates</li>";
    html += "</ol>";
    html += "</div>";

    html += "<div style='text-align:center; margin-top:24px;'>";
    html += "<a href='/' style='color:#67e8f9; text-decoration:none;'>← Back to Dashboard</a>";
    html += "</div>";

    html += "</div>"; // End content

    html += FPSTR(HTML_FOOTER);
    return html;
}

String buildUpdateSuccessPage()
{
    String html = FPSTR(HTML_HEADER);

    // Header
    html += "<div class='header'><div class='header-top'>";
    html += "<div class='header-title'><h1>SpojBoard</h1>";
    html += "<div class='header-subtitle'>Firmware Update</div></div></div></div>";

    html += "<div class='content'>";

    // Success banner with animation
    html += "<div class='banner banner-success' style='margin-bottom:24px;'>";
    html += "<div class='status-dot' style='animation: pulse 1.5s ease-in-out infinite;'></div>";
    html += "<div style='flex:1;'><strong>Update successful!</strong></div>";
    html += "</div>";

    // Update success card
    html += "<div class='card' style='border:2px solid #2ed573;'>";
    html += "<h2 style='margin-top:0; color:#2ed573; font-size:18px;'>✓ Firmware Updated</h2>";
    html += "<p style='margin:12px 0; font-size:14px;'>New firmware has been <strong style='color:#2ed573;'>uploaded and validated</strong> successfully.</p>";
    html += "<p style='color:#999; font-size:13px;'>The device will reboot to apply the new firmware. All settings and configurations will be preserved.</p>";
    html += "</div>";

    // What to expect section
    html += "<div class='card' style='background:#0a0a0a; border:1px solid #333;'>";
    html += "<h3 style='margin-top:0; font-size:14px; color:#999; text-transform:uppercase; letter-spacing:0.5px;'>What to Expect</h3>";
    html += "<ul style='margin:8px 0; padding-left:20px; color:#999; font-size:13px; line-height:1.8;'>";
    html += "<li>Device reboots in <strong style='color:#f5f5f5;'>~10 seconds</strong></li>";
    html += "<li>Firmware installation takes <strong style='color:#f5f5f5;'>15-20 seconds</strong></li>";
    html += "<li>Device boots with <strong style='color:#2ed573;'>new firmware</strong></li>";
    html += "<li>All settings and data are preserved</li>";
    html += "</ul>";
    html += "</div>";

    // Important notice
    html += "<div class='banner banner-warning' style='margin:16px 0; font-size:13px;'>";
    html += "<div>⚠️ <strong>Do not power off</strong> the device during the update process</div>";
    html += "</div>";

    // Progress bar
    html += "<div style='margin:24px 0;'>";
    html += "<div style='background:#1a1a1a; height:6px; border-radius:3px; overflow:hidden;'>";
    html += "<div id='progress-bar' style='background:linear-gradient(90deg, #67e8f9, #2ed573); height:100%; width:0%; transition:width 20s linear;'></div>";
    html += "</div>";
    html += "<div id='status-text' style='text-align:center; margin-top:8px; color:#999; font-size:12px;'>Preparing to reboot...</div>";
    html += "</div>";

    // Reconnect button (hidden initially)
    html += "<div id='reconnect-section' style='display:none; margin-top:24px;'>";
    html += "<button onclick='window.location=\"/\"' class='btn-primary' style='background:#2ed573;'>✓ Reconnect to Device</button>";
    html += "<p style='text-align:center; margin-top:12px; color:#666; font-size:12px;'>New firmware is now running</p>";
    html += "</div>";

    html += "</div>"; // End content

    // Animation and auto-reconnect script
    html += "<style>";
    html += "@keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.3; } }";
    html += "</style>";
    html += "<script>";
    html += "setTimeout(function(){ document.getElementById('progress-bar').style.width='100%'; }, 100);";
    html += "setTimeout(function(){ document.getElementById('status-text').textContent='Installing firmware...'; }, 8000);";
    html += "setTimeout(function(){ document.getElementById('status-text').textContent='Booting with new firmware...'; }, 15000);";
    html += "setTimeout(function(){ ";
    html += "  document.getElementById('reconnect-section').style.display='block';";
    html += "  document.getElementById('status-text').textContent='Update complete!';";
    html += "}, 20000);";
    html += "</script>";

    html += FPSTR(HTML_FOOTER);
    return html;
}

String buildUpdateErrorPage(const char* errorMsg)
{
    String html = FPSTR(HTML_HEADER);

    // Header
    html += "<div class='header'><div class='header-top'>";
    html += "<div class='header-title'><h1>SpojBoard</h1>";
    html += "<div class='header-subtitle'>Update Failed</div></div></div></div>";

    html += "<div class='content'>";

    // Error banner
    html += "<div class='banner banner-error' style='margin-bottom:24px;'>";
    html += "<span class='status-dot'></span>";
    html += "<div><strong>❌ Update Failed</strong></div>";
    html += "</div>";

    // Error details card
    html += "<div class='card' style='border:2px solid #fb7185;'>";
    html += "<h2 style='margin-top:0; color:#fb7185; font-size:18px;'>Error Details</h2>";
    html += "<p style='margin:12px 0; padding:12px; background:#1a1a1a; border-radius:6px; font-family:monospace; font-size:13px; color:#fb7185;'>";
    html += String(errorMsg);
    html += "</p>";
    html += "</div>";

    // Troubleshooting card
    html += "<div class='card' style='background:#0a0a0a; border:1px solid #333;'>";
    html += "<h3 style='margin-top:0; font-size:14px; color:#999; text-transform:uppercase; letter-spacing:0.5px;'>Common Issues</h3>";
    html += "<ul style='margin:8px 0; padding-left:20px; color:#999; font-size:13px; line-height:1.8;'>";
    html += "<li><strong style='color:#f5f5f5;'>Wrong file format:</strong> Ensure you're uploading a .bin file</li>";
    html += "<li><strong style='color:#f5f5f5;'>Wrong hardware variant:</strong> Verify firmware matches your device</li>";
    html += "<li><strong style='color:#f5f5f5;'>Corrupted file:</strong> Re-download the firmware and try again</li>";
    html += "<li><strong style='color:#f5f5f5;'>Insufficient space:</strong> Check available flash memory</li>";
    html += "</ul>";
    html += "</div>";

    // Actions
    html += "<div style='text-align:center; margin-top:24px;'>";
    html += "<a href='/update'><button class='btn-primary' style='margin-right:12px;'>🔄 Try Again</button></a>";
    html += "<a href='/' style='color:#67e8f9; text-decoration:none;'>← Back to Dashboard</a>";
    html += "</div>";

    html += "</div>"; // End content

    html += FPSTR(HTML_FOOTER);
    return html;
}
