# SpojBoard Troubleshooting Guide

Common issues and solutions when using SpojBoard.

## Table of Contents

- [WiFi Connection Issues](#wifi-connection-issues)
- [API Issues](#api-issues)
- [Display Issues](#display-issues) — wrong colours, blank/scrambled panels, panel arrangement
- [Firmware Update Issues](#firmware-update-issues)

## WiFi Connection Issues

### Can't Connect to AP

**Symptoms:** Unable to connect to the SpojBoard-XXXX WiFi network during setup.

**Solutions:**
- Make sure you're connecting to the right network (SpojBoard-XXXX)
- The password is case-sensitive and shown on the display
- Try forgetting the network on your device and reconnecting
- Move closer to the SpojBoard device
- Restart your phone/computer's WiFi

### Device Keeps Going to AP Mode

**Symptoms:** Device repeatedly enters AP mode instead of connecting to your WiFi.

**Solutions:**
- Double-check your WiFi password in the configuration
- Make sure your WiFi network is 2.4GHz (ESP32-S3 doesn't support 5GHz)
- Check if your router is blocking new devices (MAC filtering)
- Verify your WiFi network is visible and broadcasting SSID
- Try temporarily disabling WiFi security to test connectivity
- Check if your router has client isolation enabled (common in guest networks)

## API Issues

### "API Error: HTTP 401" (Prague Only)

**Symptoms:** Display shows "API Error: HTTP 401" or web interface reports authentication failure.

**Cause:** Invalid or missing Golemio API key.

**Solutions:**
- Verify your API key is correct at [api.golemio.cz/api-keys](https://api.golemio.cz/api-keys/)
- Generate a new API key if the old one has expired
- Make sure there are no extra spaces when entering the key
- Try copying and pasting the key directly from the Golemio website
- **Note:** Berlin (BVG) does not require an API key

### "API Error: HTTP 429" (Prague Only)

**Symptoms:** Display shows "API Error: HTTP 429" or logs show rate limiting messages.

**Cause:** Too many API requests sent to Golemio servers.

**Solutions:**
- Increase the refresh interval in web configuration (try 60 seconds or more)
- Reduce the number of stops you're querying
- Wait a few minutes before trying again
- Check if you have multiple devices using the same API key
- **Note:** BVG API (Berlin) is public and has different rate limits

### "No Departures"

**Symptoms:** Display is blank or shows "No Departures" message.

**Possible Causes & Solutions:**

1. **Incorrect Stop ID**
   - **Prague:** Verify at [data.pid.cz/stops/json/stops.json](https://data.pid.cz/stops/json/stops.json) (use GTFS ID like "U693Z2P")
   - **Berlin:** Verify at [v6.bvg.transport.rest](https://v6.bvg.transport.rest/) using `/locations` endpoint (numeric ID like "900013102")
   - Make sure you selected the correct city in configuration

2. **Wrong City Selected**
   - Check if you configured Prague stop IDs but selected Berlin as the city (or vice versa)
   - City and stop IDs must match (Prague uses GTFS IDs, Berlin uses numeric IDs)

3. **No Service at Current Time**
   - Some stops may have no service during night hours
   - Check if the stop has active service at the current time
   - Wait a few minutes and check if departures appear

4. **Minimum Departure Time Filter**
   - The configured minimum departure time may be filtering out all departures
   - Try reducing the minimum departure time in configuration (set to 0 to disable)

5. **Display Working Incorrectly**
   - Try the demo mode to verify the display hardware is functioning correctly
   - If demo works but live data doesn't, the issue is with API/configuration

## Display Issues

### Display Shows Garbled Text or Wrong Characters

**Symptoms:** Text appears corrupted, characters are wrong, or Czech diacritics don't display correctly.

**Possible Causes & Solutions:**
- Check HUB75 cable connections between panels
- Verify both panels are powered properly
- Try reseating the HUB75 cable connections
- Check if panel chain order is correct (first panel closest to controller)

### Display is Blank or Very Dim

**Symptoms:** Display doesn't show anything or is barely visible.

**Solutions:**
- Verify power supply is connected and provides at least 5V 3A
- Check the brightness setting in configuration (0-255, default 90)
- Increase brightness via web interface
- Verify HUB75 cable is properly seated in the MatrixPortal
- Check if panels are receiving power (look for indicator LEDs on back)

### Display Shows Wrong Colors

**Symptoms:** Colors appear incorrect or washed out. Orange looks pink, sky blue looks teal, yellow looks violet.

**Solutions:**
- **Start with the test pattern.** Open the **Hardware** tab and press **Test pattern**: three bars appear, labelled R, G and B. If a letter is not sitting on its own colour, the channel order is wrong — change **RGB channel order** and reboot. `RGB` is the standard cable order; `RBG` is what a MatrixPortal needs with 64×32 panels. Consistently shifted colours are almost always this, not a cable fault
- **After swapping panel heights:** the MatrixPortal's green/blue transposition applies to 64×32 panels only, so moving to 64×64 panels usually means switching the channel order to `RGB`
- Check HUB75 cable connections (loose connections can cause color issues)
- Verify RGB data pins are correctly connected
- Try reseating all connections
- **Level shifting:** If using a generic ESP32-S3 (not MatrixPortal S3), the 3.3V signal levels may be too low for your panel. HUB75 panels are 5V logic devices and some batches won't work reliably at 3.3V. Add a 74AHCT245 level shifter, or switch to a MatrixPortal S3 which includes level shifters on-board

### Panel Blank, Ghosting, Scrambled, or Half-Lit

**Symptoms:** Nothing on the panel, heavy ghosting, interleaved rows, or the top and bottom halves showing the same thing.

**Solutions:**
- **Wrong panel arrangement** is the first thing to check: **Display → Panel arrangement** must match the panels you actually have. Two of the three options are 128×64 pixels but different hardware, so count the panels. Picking wrong scrambles the image; nothing is damaged
- **Blank or ghosting** can be a driver chip that needs its own init sequence — try **Hardware → Panel driver chip** (FM6126A and ICN2038S are the common ones)
- **Top and bottom halves identical** on a 64-high panel means the E address line is not reaching it — check that wire
- **Interleaved or shredded rows** on a single 128×64 module means it uses a non-standard internal scan map, which needs firmware support. Open an issue with a photo
- If you changed the pin map by hand and the panel went dark, press **Restore built-in wiring** on the Hardware tab — it works in setup (AP) mode, so it never needs a working panel

### Physical Display Issues

**Symptoms:** Flickering, lines, or partial display.

**Solutions:**
- Check HUB75 cable connections between panels
- Verify panel chain order (panels must be connected in sequence)
- Ensure power supply can handle the current draw
- Try reducing brightness to see if power supply is insufficient
- Check for loose solder joints on panel connectors

## Firmware Update Issues

### "Updates not available in AP mode"

**Symptoms:** Update button is disabled or shows error about AP mode.

**Cause:** Firmware updates are disabled in AP mode for security.

**Solution:**
- Connect the device to your WiFi network first
- Configure WiFi credentials via the AP setup interface
- Once connected to WiFi, updates will be available

### "No update available"

**Symptoms:** Check for updates reports no new version.

**Cause:** Your device is already running the latest version.

**Solution:**
- This is normal - no action needed
- Check [GitHub Releases](https://github.com/xbach/spojboard-firmware/releases) to verify latest version

### "Download failed" or "Installation failed"

**Symptoms:** Update download or installation fails partway through.

**Possible Causes & Solutions:**

1. **Internet Connection Issues**
   - Check if device can access the internet
   - Try again - download may have been interrupted
   - Verify your router allows HTTPS connections to github.com

2. **Insufficient Space**
   - Check if device has enough space for firmware (~1-2 MB)
   - Try rebooting the device and attempting update again

3. **Corrupted Download**
   - Firmware includes MD5 validation - corrupted files are rejected
   - Simply retry the download

### "GitHub API error" or "Rate limit exceeded"

**Symptoms:** Update check fails with GitHub API error message.

**Cause:** GitHub limits unauthenticated API requests to 60 per hour.

**Solution:**
- This is normal and not a device issue
- Wait an hour and try again
- The limit applies per IP address, not per device

### Update Stuck at 0% or Progress Bar Frozen

**Symptoms:** Download starts but progress doesn't update.

**Solutions:**
- Wait at least 30 seconds - large firmware takes time to download
- Check if device has internet connectivity
- Try rebooting and attempting update again
- If repeatedly failing, try manual firmware upload instead

## Getting Help

If your issue isn't covered here:

1. **Check Serial Output**
   ```bash
   pio device monitor
   ```
   Serial logs often reveal the root cause

2. **Enable Debug Mode**
   - Enable debug mode in web configuration
   - This turns on verbose HTTP chunk and API-parse logging on the serial console

3. **Try Demo Mode**
   - Use demo mode to verify hardware is functioning
   - If demo works but live data doesn't, issue is with API/configuration

4. **Factory Reset**
   - Use the factory reset button in web interface
   - This clears all configuration and starts fresh

5. **Report Issues**
   - GitHub Issues: [github.com/xbach/spojboard-firmware/issues](https://github.com/xbach/spojboard-firmware/issues)
   - Include serial output, device logs, and steps to reproduce
