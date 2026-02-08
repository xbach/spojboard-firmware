#include "DisplayTab.h"

// Helper: Build line colors table
static String buildLineColorsTable(const Config* config)
{
    String html = "";

    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>Line Colors</div>";

    // Parse existing line color map
    String lineColorMap = String(config->lineColorMap);
    bool hasRows = lineColorMap.length() > 0;

    html += "<table>";
    html += "<thead>";
    html += "<tr>";
    html += "<th>Line</th>";
    html += "<th>Color</th>";
    html += "<th class='center'>Action</th>";
    html += "</tr>";
    html += "</thead>";
    html += "<tbody id='lineColorsTableBody'>";

    if (hasRows)
    {
        // Parse and display existing line color mappings
        // Format: "LINE=COLOR,LINE=COLOR"
        int startIdx = 0;
        while (startIdx < lineColorMap.length())
        {
            int commaIdx = lineColorMap.indexOf(',', startIdx);
            if (commaIdx == -1)
            {
                commaIdx = lineColorMap.length();
            }

            String pair = lineColorMap.substring(startIdx, commaIdx);
            int equalIdx = pair.indexOf('=');

            if (equalIdx != -1)
            {
                String line = pair.substring(0, equalIdx);
                String color = pair.substring(equalIdx + 1);

                line.trim();
                color.trim();

                html += "<tr>";
                html += "<td><input type='text' class='line-input' value='";
                html += line;
                html += "' placeholder='Line number'></td>";
                html += "<td><select class='color-select'>";
                html += "<option value='RED'";
                if (color == "RED")
                    html += " selected";
                html += ">RED</option>";
                html += "<option value='GREEN'";
                if (color == "GREEN")
                    html += " selected";
                html += ">GREEN</option>";
                html += "<option value='BLUE'";
                if (color == "BLUE")
                    html += " selected";
                html += ">BLUE</option>";
                html += "<option value='YELLOW'";
                if (color == "YELLOW")
                    html += " selected";
                html += ">YELLOW</option>";
                html += "<option value='ORANGE'";
                if (color == "ORANGE")
                    html += " selected";
                html += ">ORANGE</option>";
                html += "<option value='PURPLE'";
                if (color == "PURPLE")
                    html += " selected";
                html += ">PURPLE</option>";
                html += "<option value='CYAN'";
                if (color == "CYAN")
                    html += " selected";
                html += ">CYAN</option>";
                html += "<option value='WHITE'";
                if (color == "WHITE")
                    html += " selected";
                html += ">WHITE</option>";
                html += "</select></td>";
                html += "<td class='center'><button type='button' class='delete-btn' "
                        "onclick='deleteLineColorRow(this)'>Delete</button></td>";
                html += "</tr>";
            }

            startIdx = commaIdx + 1;
        }
    }
    else
    {
        // Empty state message
        html += "<tr id='emptyState'>";
        html += "<td colspan='3' style='text-align:center;color:#666;padding:20px;'>";
        html += "No line colors configured. Click \"+ Add Line Color\" to add one.";
        html += "</td>";
        html += "</tr>";
    }

    html += "</tbody>";
    html += "</table>";

    html += "<button type='button' class='secondary' onclick='addLineColorRow()'>+ Add Line Color</button>";

    // Hidden input to store serialized data
    html += "<input type='hidden' id='lineColorMapData' name='line_color_map' value=''>";

    html += "</div>"; // End form-group

    return html;
}

// Main builder
String buildDisplayTab(const Config* config)
{
    String html = "";
    html += "<div id='tab-display' class='tab-content'>";

    // Basic Display Settings (2-column grid)
    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>Basic Display Settings</div>";
    html += "<div class='grid'>";

    // Brightness slider
    html += "<div>";
    html += "<label for='brightness'>BRIGHTNESS</label>";
    html += "<input type='range' id='brightness' name='brightness' min='0' max='255' value='";
    html += String(config->brightness);
    html += "'>";
    html += "<div class='help-text'>Current: <span id='brightnessValue'>";
    html += String(config->brightness);
    html += "</span></div>";
    html += "</div>";

    // Number of departures
    html += "<div>";
    html += "<label for='numDepartures'>NUMBER OF DEPARTURES</label>";
    html += "<input type='number' id='numDepartures' name='num_deps' min='1' max='3' value='";
    html += String(config->numDepartures);
    html += "'>";
    html += "</div>";

    html += "</div>"; // End grid
    html += "</div>"; // End form-group

    // Locale selector
    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>Locale</div>";
    html += "<label for='language'>LANGUAGE</label>";
    html += "<select id='language' name='language'>";
    html += "<option value='en'";
    if (strcmp(config->language, "en") == 0)
        html += " selected";
    html += ">English</option>";
    html += "<option value='cs'";
    if (strcmp(config->language, "cs") == 0)
        html += " selected";
    html += ">Czech</option>";
    html += "<option value='de'";
    if (strcmp(config->language, "de") == 0)
        html += " selected";
    html += ">German</option>";
    html += "</select>";
    html += "</div>"; // End form-group

    // Display Options (checkboxes)
    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>Display Options</div>";

    html += "<div>";
    html += "<label><input type='checkbox' name='show_platform'";
    if (config->showPlatform)
        html += " checked";
    html += "> Show platform number</label>";
    html += "</div>";

    html += "<div>";
    html += "<label><input type='checkbox' name='scroll_enabled'";
    if (config->scrollEnabled)
        html += " checked";
    html += "> Enable scrolling</label>";
    html += "<div class='help-text'>Scroll long destination names horizontally</div>";
    html += "</div>";

    html += "<div>";
    html += "<label><input type='checkbox' name='show_multi_times'";
    if (config->showMultipleTimes)
        html += " checked";
    html += "> Show multiple departure times</label>";
    html += "<div class='help-text'>Show next two departure times per line</div>";
    html += "</div>";

    html += "</div>"; // End form-group

    // Line Colors Configuration
    html += buildLineColorsTable(config);

    html += "</div>"; // End tab-display

    return html;
}
