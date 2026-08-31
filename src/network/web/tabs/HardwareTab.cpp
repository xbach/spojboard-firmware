#include "HardwareTab.h"
#include "../WebUtils.h"
#include "../../../config/AppConfig.h"

namespace
{
struct OrderChoice
{
    const char* value;
    const char* label;
};

// The two named entries are the ones that correspond to real hardware; the rest
// exist because panels in the wild use them and guessing is cheap when the
// setting is one reboot away.
const OrderChoice ORDER_CHOICES[] = {
    {"0", "RGB - standard HUB75 cable"},
    {"1", "RBG - MatrixPortal + 64x32 panels"},
    {"2", "GRB"},
    {"3", "GBR"},
    {"4", "BRG"},
    {"5", "BGR"},
};

const char* DRIVER_CHOICES[] = {
    "SHIFTREG - plain shift register (default)",
    "FM6124",
    "FM6126A",
    "ICN2038S",
    "MBI5124",
    "DP3246",
};

String pinInput(const char* name, const char* label, int8_t value)
{
    String html = "<div class='pin-cell'>";
    html += "<label for='" + String(name) + "'>" + String(label) + "</label>";
    html += "<input type='number' id='" + String(name) + "' name='" + String(name) + "'";
    html += " min='0' max='48' value='" + String((int)value) + "'>";
    html += "</div>";
    return html;
}
} // namespace

String buildHardwareTab(const Config* config)
{
    String html = "<div id='tab-hardware' class='tab-content'>";

    // ---------------------------------------------------------- channel order
    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>Panel Wiring</div>";
    html += "<div class='help-text' style='margin-bottom:10px;'>Changes here take effect after a "
            "reboot. If the panel shows wrong colours - orange looking pink, sky looking teal - the "
            "channel order is what to change, not the pin numbers.</div>";

    html += "<label for='hw_rgb_order'>RGB CHANNEL ORDER</label>";
    html += "<select id='hw_rgb_order' name='hw_rgb_order'>";
    for (const OrderChoice& c : ORDER_CHOICES)
    {
        html += "<option value='" + String(c.value) + "'";
        if ((int)config->hwProfile.order == atoi(c.value))
        {
            html += " selected";
        }
        html += ">" + String(c.label) + "</option>";
    }
    html += "</select>";

    // ------------------------------------------------------------ driver chip
    html += "<label for='hw_driver'>PANEL DRIVER CHIP</label>";
    html += "<select id='hw_driver' name='hw_driver'>";
    for (uint8_t i = 0; i < sizeof(DRIVER_CHOICES) / sizeof(DRIVER_CHOICES[0]); i++)
    {
        html += "<option value='" + String(i) + "'";
        if (config->hwProfile.driver == i)
        {
            html += " selected";
        }
        html += ">" + String(DRIVER_CHOICES[i]) + "</option>";
    }
    html += "</select>";
    html += "<div class='help-text'>Leave on SHIFTREG unless the panel stays blank or ghosts - some "
            "panels need a chip-specific init sequence.</div>";
    html += "</div>"; // form-group

    // -------------------------------------------------------- custom pin map
    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>Advanced: Custom Pin Map</div>";
    html += "<div class='help-text' style='margin-bottom:10px;'>Only needed for hand-wired boards. "
            "Pins are GPIO numbers in HUB75 connector order; the channel order above is applied on "
            "top of them. A map that fails validation is ignored and the built-in one is used "
            "instead, so a bad guess cannot leave the panel dark forever.</div>";

    html += "<label class='checkbox-label'>";
    html += "<input type='checkbox' id='hw_custom_pins' name='hw_custom_pins' value='1'";
    if (config->hwProfile.useCustomPins)
    {
        html += " checked";
    }
    html += "> Use a custom pin map</label>";

    const HubPins& p = config->hwProfile.pins;
    html += "<div class='pin-grid'>";
    html += pinInput("hw_r1", "R1", p.r1);
    html += pinInput("hw_g1", "G1", p.g1);
    html += pinInput("hw_b1", "B1", p.b1);
    html += pinInput("hw_r2", "R2", p.r2);
    html += pinInput("hw_g2", "G2", p.g2);
    html += pinInput("hw_b2", "B2", p.b2);
    html += pinInput("hw_a", "A", p.a);
    html += pinInput("hw_b", "B", p.b);
    html += pinInput("hw_c", "C", p.c);
    html += pinInput("hw_d", "D", p.d);
    html += pinInput("hw_e", "E", p.e);
    html += pinInput("hw_lat", "LAT", p.lat);
    html += pinInput("hw_oe", "OE", p.oe);
    html += pinInput("hw_clk", "CLK", p.clk);
    html += "</div>";
    html += "</div>"; // form-group

    html += "</div>"; // tab-hardware
    return html;
}
