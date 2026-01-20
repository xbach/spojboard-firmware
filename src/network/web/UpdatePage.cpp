#include "UpdatePage.h"
#include "WebTemplates.h"
#include "ClientScripts.h"
#include "../../config/AppConfig.h"

String buildUpdatePage()
{
    String html = FPSTR(HTML_HEADER);
    html += "<h1>Firmware Update</h1>";

    // Warning card
    html += "<div class='card' style='background: #ff6b6b; color: #fff;'>";
    html += "<h3 style='color: #fff; margin-top: 0;'>Important</h3>";
    html += "<ul style='margin: 10px 0; padding-left: 20px;'>";
    html += "<li>Do NOT power off or disconnect during update!</li>";
    html += "<li>Update takes 1-2 minutes to complete</li>";
    html += "<li>Device will reboot automatically after update</li>";
    html += "<li>Make sure you upload the correct .bin file for ESP32-S3</li>";
    html += "</ul>";
    html += "</div>";

    // Current firmware info
    html += "<div class='card'>";
    html += "<h2>Current Firmware</h2>";
    char currentBuildId[10];
    snprintf(currentBuildId, sizeof(currentBuildId), "%08x", BUILD_ID);
    html += "<p><strong>Release:</strong> " + String(FIRMWARE_RELEASE) + "</p>";
    html += "<p><strong>Build ID:</strong> " + String(currentBuildId) + "</p>";
    html += "</div>";

    // Upload form
    html += "<div class='card'>";
    html += "<h2>Upload New Firmware</h2>";
    html += "<form method='POST' action='/update' enctype='multipart/form-data' id='uploadForm'>";
    html += "<input type='file' name='firmware' accept='.bin' required style='margin-bottom: 15px;'>";
    html += "<button type='submit' id='uploadBtn'>Upload Firmware</button>";
    html += "</form>";
    html += "<div id='progress' style='display:none; margin-top:20px;'>";
    html += "<div style='background:#333; border-radius:5px; overflow:hidden; height:30px;'>";
    html += "<div id='progressBar' style='background:#00d4ff; height:100%; width:0%; transition:width 0.3s;'></div>";
    html += "</div>";
    html += "<p id='progressText' style='text-align:center; margin-top:10px;'>Uploading...</p>";
    html += "</div>";
    html += "</div>";

    // JavaScript for progress
    html += FPSTR(SCRIPT_OTA_UPLOAD);

    html += "<p><a href='/'>Back to Dashboard</a></p>";
    html += FPSTR(HTML_FOOTER);
    return html;
}

String buildUpdateBlockedPage()
{
    String html = FPSTR(HTML_HEADER);
    html += "<h1>OTA Update Unavailable</h1>";
    html += "<div class='card' style='background: #ff6b6b; color: #fff;'>";
    html += "<p>Firmware updates are disabled in AP (setup) mode for security reasons.</p>";
    html += "<p>Please connect the device to your WiFi network first.</p>";
    html += "</div>";
    html += "<p><a href='/'>Back to Dashboard</a></p>";
    html += FPSTR(HTML_FOOTER);
    return html;
}

String buildUpdateSuccessPage()
{
    String html = FPSTR(HTML_HEADER);
    html += "<h1>Update Successful!</h1>";
    html += "<div class='card' style='background: #2ed573; color: #000;'>";
    html += "<p>Firmware has been uploaded and validated successfully.</p>";
    html += "<p>The device will reboot in 10 seconds. Please wait 15-20 seconds for it to come back online.</p>";
    html += "</div>";
    html += "<div id='reconnect-msg' style='display:none; margin-top:20px;'>";
    html += "<p><button onclick='window.location=\"/\"' style='padding:12px 24px; font-size:16px; cursor:pointer; background:#2ed573; color:#000; border:none; border-radius:8px;'>Reconnect to Device</button></p>";
    html += "</div>";
    html += "<script>setTimeout(function(){ document.getElementById('reconnect-msg').style.display='block'; }, 15000);</script>";
    html += FPSTR(HTML_FOOTER);
    return html;
}

String buildUpdateErrorPage(const char* errorMsg)
{
    String html = FPSTR(HTML_HEADER);
    html += "<h1>Update Failed</h1>";
    html += "<div class='card' style='background: #ff6b6b; color: #fff;'>";
    html += "<p><strong>Error:</strong> " + String(errorMsg) + "</p>";
    html += "</div>";
    html += "<p><a href='/update'>Try Again</a></p>";
    html += "<p><a href='/'>Back to Dashboard</a></p>";
    html += FPSTR(HTML_FOOTER);
    return html;
}
