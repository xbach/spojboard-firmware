#!/bin/bash
# Test script to hit the Prague Golemio API locally
# Uses the same endpoint and parameters as GolemioAPI.cpp

API_KEY="${GOLEMIO_API_KEY:?Set GOLEMIO_API_KEY env var (get one at https://api.golemio.cz/api-keys)}"

# Default stop ID: Andel B (tram stop near Andel metro)
STOP_ID="${1:-U321Z2P}"
TOTAL=12
MINUTES_BEFORE=0
MINUTES_AFTER=120

URL="https://api.golemio.cz/v2/pid/departureboards?ids=${STOP_ID}&total=${TOTAL}&preferredTimezone=Europe/Prague&minutesBefore=${MINUTES_BEFORE}&minutesAfter=${MINUTES_AFTER}"

echo "=== Golemio API Test ==="
echo "Stop ID: ${STOP_ID}"
echo "URL: ${URL}"
echo ""

curl -s \
  -H "x-access-token: ${API_KEY}" \
  -H "Content-Type: application/json" \
  "${URL}" | python3 -m json.tool
