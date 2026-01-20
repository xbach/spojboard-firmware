# MQTT Transit API Documentation

## Overview

SpojBoard's MQTT API enables integration with any server that can provide transit data over MQTT. Instead of querying transit APIs directly, SpojBoard sends an MQTT request and receives departure data from your server, which can aggregate data from multiple sources, apply custom filtering, and reduce API calls.

**Any MQTT-capable server will work**, as long as it can:
- Provide transit/transport data (from any source: APIs, databases, scraping, etc.)
- Communicate over MQTT protocol
- Keep data fresh (update regularly)
- Listen for SpojBoard requests and respond with JSON

This document uses **Home Assistant as an example implementation**, but the same principles apply to custom Python scripts, Node-RED flows, or any other MQTT-enabled system.

## Architecture

```
┌──────────────┐                    ┌──────────────────┐                  ┌─────────────────────┐
│  SpojBoard   │                    │  MQTT Broker     │                  │   Your Server       │
│              │                    │  (Mosquitto)     │                  │ (HA, Python, etc.)  │
│              │                    │                  │                  │                     │
│   1. Publish │───"request"──────>│  Request Topic   │──── Trigger ────>│ Request Handler     │
│              │                    │                  │                  │                     │
│              │                    │                  │                  │        ↓            │
│              │                    │                  │                  │  Fetch/Format       │
│              │                    │                  │                  │  Transit Data       │
│              │                    │                  │                  │        ↓            │
│   2. Receive │<───JSON response──│  Response Topic  │<─── Publish ─────│ MQTT Publish        │
│              │                    │                  │                  │                     │
│   3. Parse   │                    │                  │                  │                     │
│   4. Display │                    │                  │                  │                     │
└──────────────┘                    └──────────────────┘                  └─────────────────────┘
     Every                              Always on                          Reactive
  60s (config)                                                          (on SpojBoard request)
```

**Request/Response Pattern:**
1. SpojBoard publishes `"request"` to request topic every N seconds (configurable)
2. Your server detects the request (via MQTT subscription)
3. Server fetches/formats transit data (from APIs, databases, cache, etc.)
4. Server publishes JSON response to response topic
5. SpojBoard receives, parses, and displays departures

**Server Implementation Examples:**
- **Home Assistant**: Binary sensor + automation + template sensor (detailed example below)
- **Python script**: MQTT client subscribing to request topic, responding with transit data
- **Node-RED**: Flow with MQTT input → API call → JSON formatting → MQTT output
- **Custom daemon**: Any service that can listen to MQTT and provide transit JSON

## Key Features

### Dual ETA Modes

**Timestamp Mode** (Recommended):
- Server sends unix timestamps (`"dep": 1768154164`)
- SpojBoard recalculates ETAs every 10 seconds automatically
- ✅ **Benefit**: Longer refresh intervals (60-300s) - server can send many departures, device handles countdown
- Use when: Server has accurate departure times

**ETA Mode**:
- Server sends pre-calculated minutes (`"eta": 5`)
- SpojBoard displays as-is without recalculation
- ⚠️ **Limitation**: Requires frequent refresh (10-60s) to stay accurate
- Use when: Server only has relative ETAs, not absolute times

### Configurable JSON Field Mappings

All JSON field names are user-configurable, allowing integration with any MQTT schema:
- Line number field (default: `"line"`)
- Destination field (default: `"dest"`)
- ETA field (default: `"eta"`)
- Timestamp field (default: `"dep"`)
- Platform field (default: `"plt"`, optional)
- AC flag field (default: `"ac"`, optional)

### Dual Filtering Strategy

**Important:** Minimum departure time filtering happens in **two places**:

1. **Server-side** (initial filter): Filter departures before sending to keep MQTT payloads small
2. **Device-side** (recalculation filter): Applied every 10 seconds during ETA recalculation

**Why both?**
- Server sends departures with timestamps (e.g., departures at 14:03, 14:05, 14:08)
- Server filters at 14:00: All pass if minDepTime=3 minutes
- SpojBoard recalculates every 10s
- At 14:01, the 14:03 departure is now 2 minutes away → filtered out by device

**Setup:**
- Configure the same minDepartureTime value on both server (template filter) and SpojBoard (web config)
- Recommended: 3 minutes minimum to avoid "departing now" entries

## SpojBoard Configuration

### 1. Access Web Interface

Navigate to `http://[device-ip]/` in your browser.

### 2. Select MQTT City

In the **City Selector**, choose **MQTT (Custom)**.

### 3. Configure MQTT Broker

**MQTT Broker Settings:**
- **Broker Address**: IP or hostname of MQTT broker (e.g., `192.168.1.100` or `homeassistant.local`)
- **Broker Port**: Default `1883` (or `8883` for TLS, if supported)
- **Username**: Optional (leave empty for no authentication)
- **Password**: Optional (leave empty for no authentication)

**MQTT Topics:**
- **Request Topic**: Where SpojBoard publishes requests (e.g., `home/spojboard/request`)
- **Response Topic**: Where SpojBoard listens for JSON responses (e.g., `home/spojboard/departures`)

### 4. Select ETA Mode

**Timestamp Mode** (Recommended):
- Server provides unix timestamps
- Device recalculates ETAs every 10s
- Set refresh interval: **60-300 seconds**

**ETA Mode**:
- Server provides pre-calculated minutes
- Device displays without recalculation
- Set refresh interval: **10-60 seconds**

### 5. Configure JSON Field Mappings

Map your JSON structure to SpojBoard fields:

| SpojBoard Field | Your JSON Field | Required | Example Value |
|----------------|-----------------|----------|---------------|
| Line Number | `line` | ✅ Yes | `"12"`, `"A"`, `"S1"` |
| Destination | `dest` | ✅ Yes | `"Sídl. Barrandov"` |
| ETA (minutes) | `eta` | Timestamp mode: No<br>ETA mode: Yes | `5` |
| Timestamp (unix) | `dep` | Timestamp mode: Yes<br>ETA mode: No | `1768154164` |
| Platform | `plt` | ❌ No | `"D"`, `"3"` |
| AC Flag | `ac` | ❌ No | `true`, `false` |

### 6. Display Settings

- **Refresh Interval**:
  - Timestamp mode: 60-300s (device handles countdown)
  - ETA mode: 10-60s (needs frequent server updates)
- **Number of Departures**: 1-3 rows (max visible on 128×32 display)
- **Min Departure Time**: ⚠️ **Set the same value on both server and device** - See Dual Filtering Strategy above

### 7. Save Configuration

Click **Save Configuration**. Device will apply settings and begin polling MQTT.

## Expected JSON Response Format

SpojBoard expects a JSON object with a `"departures"` array:

```json
{
  "departures": [
    {
      "line": "12",
      "dest": "Sídl. Barrandov",
      "eta": 5,
      "dep": 1768154164,
      "plt": "D",
      "ac": false
    },
    {
      "line": "A",
      "dest": "Depo Hostivař",
      "eta": 7,
      "dep": 1768154284,
      "plt": "1",
      "ac": true
    }
  ]
}
```

**Field Notes:**
- `eta` and `dep`: Only one is required based on ETA mode selection
- `plt`: Optional, stored but not currently displayed
- `ac`: Optional, shows ❄️ icon on display if `true`
- Additional fields in JSON are ignored

## Generic Server Requirements

**You can use any server that can:**
1. **Subscribe to MQTT topics** - Listen for SpojBoard's "request" message
2. **Fetch transit data** - From any source (APIs, databases, web scraping, local cache)
3. **Format JSON** - Create the departures array shown above
4. **Publish to MQTT** - Send the JSON response to SpojBoard
5. **Keep data fresh** - Update regularly to show current departures

**Implementation options:**
- **Home Assistant** - Template sensors + automations (detailed example below)
- **Python script** - paho-mqtt client + API calls + JSON formatting
- **Node-RED** - Visual flow programming with MQTT nodes
- **Custom daemon** - Any language with MQTT library (Go, Rust, JavaScript, etc.)
- **Shell script** - mosquitto_pub + curl + jq for simple setups

**Key considerations:**
- Response time: Aim for <2 seconds from request to response
- Data freshness: Update your transit data source frequently (every 15-30 seconds recommended)
- Filtering: Apply minimum departure time filter server-side to reduce MQTT payload size
- Error handling: Return empty departures array if no data available (don't fail to respond)

## Server Implementation Examples

This section provides detailed examples of how to implement an MQTT server for SpojBoard. **Home Assistant is shown as the primary example**, but the same concepts apply to any MQTT-capable system.

### Example 1: Home Assistant

**Prerequisites:**
- Home Assistant with MQTT integration enabled
- MQTT broker (Mosquitto addon recommended)
- Transit data sensors (e.g., PID, GTFS, custom API integration)

### 1. Configure MQTT Binary Sensor

Add to `configuration.yaml`:

```yaml
mqtt:
  binary_sensor:
    - name: "SpojBoard Request"
      unique_id: spojboard_request
      state_topic: "home/spojboard/request"
      payload_on: "request"
      payload_off: "OFF"
      value_template: "{{ value }}"
      off_delay: 2
```

**Explanation:**
- **state_topic**: Must match SpojBoard's **Request Topic**
- **payload_on**: Must be `"request"` (case-sensitive!)
- Triggers to `"on"` state when SpojBoard publishes request

### 2. Create Template Sensor for Departures

This sensor formats your transit data into SpojBoard's expected JSON format.

#### Example: [Prague PID Integration for HA](https://github.com/dvejsada/PID_integration/tree/master)

Add to `configuration.yaml`:

```yaml
template:
  # Template sensor to format departures for SpojBoard
  - trigger:
      - platform: time_pattern
        seconds: "/15"  # Update every 15 seconds
    sensor:
      - name: "SpojBoard Departures JSON"
        unique_id: spojboard_departures_json
        state: "{{ now().isoformat() }}"
        attributes:
          departures: >
            {% set ns = namespace(departures=[]) %}

            {# Get all PID sensors for your stops #}
            {% set entities = states.sensor
              | selectattr('entity_id', 'search', '(U_Prdlavky_A|U_Prdlavky_B).*next_route_name')
              | selectattr('attributes.departure_time_est', 'defined')
              | list %}

            {# Sort by departure time #}
            {% set sorted = entities
              | sort(attribute='attributes.departure_time_est') %}

            {# Format top 8 departures #}
            {% for entity in sorted[:8] %}
              {% set line = entity.state %}
              {% set dest = entity.attributes.trip_headsign %}
              {% set dep_time = as_timestamp(entity.attributes.departure_time_est) %}
              {% set now_time = as_timestamp(now()) %}
              {% set eta = ((dep_time - now_time) / 60) | int %}
              {% set plt = entity.attributes.stop_platform %}

              {# Filter: Only include departures ≥3 minutes #}
              {% if eta >= 3 %}
                {% set ns.departures = ns.departures + [{
                  'line': line,
                  'dest': dest,
                  'eta': eta,
                  'dep': dep_time | int,
                  'plt': plt,
                  'ac': false
                }] %}
              {% endif %}
            {% endfor %}

            {# Return all #}
            {{ ns.departures }}
```

**Customization Points:**
- **`selectattr(..., 'search', '...')`**: Replace with your sensor entity pattern
- **Destination formatting**: Modify replacements, truncation as needed
- **Platform logic**: Customize `plt` field based on your stop mapping
- **Minimum ETA filter**: `{% if eta >= 3 %}` - ⚠️ **Must match SpojBoard's minDepartureTime setting** (see Dual Filtering Strategy)
- **Return count**: `{{ ns.departures[:3] }}` - adjust number based on how many rows you want to send to device

#### Example: Generic GTFS/Custom API

If you have custom sensors with different attributes:

```yaml
template:
  - trigger:
      - platform: time_pattern
        seconds: "/30"
    sensor:
      - name: "SpojBoard Departures JSON"
        unique_id: spojboard_departures_json
        state: "{{ now().isoformat() }}"
        attributes:
          departures: >
            {% set ns = namespace(departures=[]) %}

            {# Example: Assume you have sensor.bus_12, sensor.tram_a, etc. #}
            {% set sources = [
              states.sensor.bus_12,
              states.sensor.tram_a,
              states.sensor.metro_green
            ] %}

            {% for source in sources if source.state not in ['unknown', 'unavailable'] %}
              {% set ns.departures = ns.departures + [{
                'line': source.attributes.route,
                'dest': source.attributes.destination,
                'dep': as_timestamp(source.attributes.departure_time) | int,
                'plt': source.attributes.platform | default(''),
                'ac': source.attributes.has_ac | default(false)
              }] %}
            {% endfor %}

            {{ ns.departures | sort(attribute='dep') | list }}
```

### 3. Create Automation to Publish to MQTT

This automation responds to SpojBoard requests by publishing the formatted JSON.

Add via **Settings → Automations → Create Automation → YAML**:

```yaml
alias: SpojBoard - Send Departures
description: "Respond to SpojBoard MQTT requests with departure data"
triggers:
  - trigger: state
    entity_id:
      - binary_sensor.spojboard_request
    to: "on"
conditions: []
actions:
  - action: mqtt.publish
    metadata: {}
    data:
      topic: home/spojboard/departures
      retain: false
      evaluate_payload: false
      payload: |-
        {{ {
          'departures': state_attr('sensor.spojboard_departures_json', 'departures'),
          'updated': now().isoformat()
        } | to_json }}
mode: single
```

**Important:**
- **topic**: Must match SpojBoard's **Response Topic**
- **evaluate_payload**: Set to `false` to use template
- **payload**: Converts sensor attribute to JSON string

### 4. Verify Setup

#### Check MQTT Messages

Use MQTT Explorer or `mosquitto_sub`:

```bash
# Terminal 1: Monitor requests from SpojBoard
mosquitto_sub -h localhost -t "home/spojboard/request" -v

# Terminal 2: Monitor responses to SpojBoard
mosquitto_sub -h localhost -t "home/spojboard/departures" -v
```

#### Check Home Assistant Logs

Go to **Developer Tools → Template** and test your sensor template:

```jinja2
{{ state_attr('sensor.spojboard_departures_json', 'departures') }}
```

Should return an array of departure objects.

#### Check SpojBoard Serial Output

Connect to serial monitor at 115200 baud:

```
[0000171410] MQTT: Publishing to home/spojboard/request
[0000171480] MQTT: Message received (279 bytes)
[0000171496] MQTT: Parsing 3 departures
[0000171529] MQTT: Successfully parsed 3 departures
[0000171634] MQTT: Received 3 departures
[0000181530] ETA Recalc: 3 deps -> 3 valid
```

## Troubleshooting

### SpojBoard Not Receiving Messages

**Symptom**: Logs show "Response timeout"

**Checks:**
1. Verify MQTT broker is running: `systemctl status mosquitto`
2. Check binary sensor state in Home Assistant: Should toggle to `"on"` when SpojBoard requests
3. Check automation is enabled and triggering
4. Verify topic names match exactly (case-sensitive!)
5. Test manual publish:
   ```bash
   mosquitto_pub -h localhost -t "home/spojboard/departures" -m '{"departures":[{"line":"12","dest":"Test","dep":1768154164}]}'
   ```

### Empty Departures Array

**Symptom**: MQTT: Parsing 0 departures

**Checks:**
1. Verify template sensor has data: `{{ state_attr('sensor.spojboard_departures_json', 'departures') }}`
2. Check transit sensors are updating (not `unknown` or `unavailable`)
3. Verify minimum ETA filter isn't too restrictive
4. Check time synchronization (NTP) on both SpojBoard and Home Assistant

### Departures Not Counting Down

**Symptom**: ETAs stay static

**For Timestamp Mode:**
- Verify `dep` field contains unix timestamp (integer, ~1768154164)
- Check SpojBoard logs show "ETA Recalc" every 10 seconds
- Ensure NTP is synced on SpojBoard

**For ETA Mode:**
- Reduce refresh interval to 10-30 seconds
- Verify server is recalculating ETAs, not caching old values

### Departures Below Minimum Time Still Showing

**Symptom**: Departures with ETA <3 minutes appear on display

**Cause**: Mismatch between server and device filtering thresholds

**Fix:**
1. Check SpojBoard web config: **Min Departure Time** value (e.g., 3)
2. Check Home Assistant template: `{% if eta >= 3 %}` filter value
3. Ensure both use the **same threshold**
4. Update template to match SpojBoard setting
5. Restart Home Assistant to reload template
6. Wait for next SpojBoard refresh cycle

**Verification:**
- Monitor logs: Should see "ETA Recalc: X deps -> Y valid (filtered Z)" when departures are removed
- If no filtering happens, check that departures have valid `dep` timestamps for recalculation

### Wrong Field Displayed

**Symptom**: Line shows destination, or gibberish

**Checks:**
1. Verify JSON field mappings in SpojBoard config match your template
2. Check JSON response format with MQTT Explorer
3. Ensure field names are exact (case-sensitive!)

### Home Assistant Authentication Issues

**Symptom**: "MQTT: Connection failed"

**Checks:**
1. If MQTT broker requires auth, configure Username/Password in SpojBoard
2. Verify credentials: `mosquitto_sub -h localhost -u username -P password -t "#"`
3. Check broker ACL if using access control

## Advanced Configuration

### Multiple SpojBoards

Run multiple boards with separate request topics:

**Board 1:**
- Request: `home/spojboard1/request`
- Response: `home/spojboard1/departures`

**Board 2:**
- Request: `home/spojboard2/request`
- Response: `home/spojboard2/departures`

Create separate binary sensors and automations for each board.

### Stop-Specific Departures

Use different response topics per board to show different stops:

```yaml
# Automation 1: Living Room board (Stop A)
- alias: SpojBoard Living Room
  triggers:
    - trigger: state
      entity_id: binary_sensor.spojboard_living_room
      to: "on"
  actions:
    - action: mqtt.publish
      data:
        topic: home/spojboard/living_room/departures
        payload: >
          {{ state_attr('sensor.stop_a_departures', 'departures') | to_json }}

# Automation 2: Kitchen board (Stop B)
- alias: SpojBoard Kitchen
  triggers:
    - trigger: state
      entity_id: binary_sensor.spojboard_kitchen
      to: "on"
  actions:
    - action: mqtt.publish
      data:
        topic: home/spojboard/kitchen/departures
        payload: >
          {{ state_attr('sensor.stop_b_departures', 'departures') | to_json }}
```

### Custom Filtering Logic

Example: Show only buses during rush hour, add metro otherwise:

```jinja2
{% set hour = now().hour %}
{% set is_rush_hour = (hour >= 7 and hour <= 9) or (hour >= 16 and hour <= 19) %}

{% if is_rush_hour %}
  {# Rush hour: Only buses #}
  {% set entities = states.sensor
    | selectattr('entity_id', 'search', 'bus_.*')
    | list %}
{% else %}
  {# Off-peak: All transit #}
  {% set entities = states.sensor
    | selectattr('entity_id', 'search', '(bus|tram|metro)_.*')
    | list %}
{% endif %}
```

### Timezone Considerations

SpojBoard uses system time (NTP synced to configured timezone). Ensure your Home Assistant timestamps use the same timezone or convert:

```jinja2
{% set dep_time = as_timestamp(entity.attributes.departure_time) %}
{# If timezone differs, adjust: #}
{% set dep_time_utc = as_timestamp(entity.attributes.departure_time | as_datetime | as_local) %}
```

## Performance Recommendations

### Refresh Intervals

| Scenario | Recommended Interval | Reasoning |
|----------|---------------------|-----------|
| Timestamp mode, 8+ departures | 120-300s | Device handles countdown, server only needs to refresh when schedule changes |
| Timestamp mode, 3-5 departures | 60-120s | Moderate refresh to ensure new departures appear |
| ETA mode | 15-60s | Server must recalculate, more frequent updates needed |

### Template Sensor Update Frequency

- Update template sensor **more frequently** than SpojBoard refresh (e.g., every 15s)
- Ensures fresh data is always available when SpojBoard requests
- Does not increase MQTT traffic (only published on request)

### Minimum Departure Time Filtering

**Dual filtering is required** - filter in both places with the same threshold:

**1. Server-side filter** (in template sensor):

```jinja2
{% if eta >= 3 %}  {# Match SpojBoard minDepartureTime setting #}
  {% set ns.departures = ns.departures + [departure] %}
{% endif %}
```

Server-side benefits:
- Smaller MQTT messages (faster parsing)
- Less RAM usage on SpojBoard
- Reduced network traffic

**2. Device-side filter** (automatic during recalculation):
- SpojBoard applies `config.minDepartureTime` every 10 seconds
- Filters out departures that drop below threshold as time passes
- Set via web interface: **Min Departure Time** setting

**Example scenario** (minDepartureTime = 3):
- **14:00**: Server sends 14:03 (3min), 14:05 (5min) → both pass server filter
- **14:00-14:01**: Device shows both departures
- **14:01**: Recalc filters 14:03 (now 2min < 3min threshold) → only 14:05 shown
- **14:05**: Next MQTT refresh brings fresh departures

**Setup:**
- Configure the **same value** on both server template and SpojBoard web config
- Recommended: 3 minutes minimum

## Implementation Details

### Buffer Sizes

- **MQTT Buffer**: 8KB (supports large JSON responses)
- **JSON Parser**: 8KB (ArduinoJson DynamicJsonDocument)
- **Max Departures**: 144 temporary (reduced to 3-12 for display)

### Connection Behavior

1. **On Request**: Connect to broker, subscribe to response topic
2. **Publish**: Send "request" message
3. **Wait**: 10-second timeout for response
4. **Parse**: Deserialize JSON, extract fields, calculate ETAs
5. **Disconnect**: Close connection (reconnects on next cycle)

### ETA Recalculation (Timestamp Mode)

Every 10 seconds (in `main.cpp`):
1. Loop through all stored departures
2. Recalculate `eta = (departureTime - now) / 60`
3. Filter out departures with `eta < config.minDepartureTime` (e.g., <3 minutes)
4. Re-sort by ETA ascending
5. Trigger display update

**Note:** This is why dual filtering is necessary - server filters at send time, device filters during recalculation as departures age.

### Memory Usage

- Config struct: +580 bytes (14 MQTT fields)
- Departure struct: +8 bytes per departure (`platform[8]` field)
- Peak heap during API call: ~24KB (JSON + MQTT buffers)
- Free heap after call: ~150KB (safe margin)

## Security Considerations

### MQTT Authentication

Use username/password authentication to prevent unauthorized access:

```yaml
# mosquitto.conf
allow_anonymous false
password_file /etc/mosquitto/passwd
```

Generate password file:
```bash
mosquitto_passwd -c /etc/mosquitto/passwd spojboard
```

### Network Isolation

- Run MQTT broker on local network only (bind to LAN interface)
- Use firewall rules to block external access to port 1883
- Consider VLANs to isolate IoT devices

### TLS/SSL

⚠️ **Currently not supported** by SpojBoard MQTT implementation. Future enhancement.

## Comparison: MQTT vs Direct API

| Feature | MQTT API | Direct API (Prague/Berlin) |
|---------|----------|---------------------------|
| **Setup Complexity** | Medium (requires MQTT server) | Low (just API key) |
| **API Calls** | Reduced (aggregated by server) | Direct (one per refresh) |
| **Flexibility** | High (custom filtering, multi-source) | Low (API-specific) |
| **Latency** | Low (local network) | Medium (internet) |
| **Dependencies** | MQTT broker + server (HA/Python/etc.) | Internet connection only |
| **Offline Resilience** | High (cached in server) | None (requires API) |
| **Multi-Stop** | Easy (server aggregates) | Limited (API constraints) |
| **Custom Data** | Full control (server-side logic) | API-provided only |

## Related Documentation

- [Main README](../README.md) - Project overview
- [Configuration Guide](CONFIGURATION.md) - General device setup
- [API Documentation](API.md) - Prague/Berlin API details
- [Hardware Setup](HARDWARE.md) - Physical assembly guide

## Support

For issues or questions:
- GitHub Issues: [spojboard-firmware/issues](https://github.com/xbach/spojboard-firmware/issues)
- Home Assistant Community: [MQTT Integration](https://community.home-assistant.io/c/configuration/mqtt/30)
