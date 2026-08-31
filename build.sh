#!/bin/bash
# Build SpojBoard firmware locally
# Supports multiple hardware variants

set -e

# Build environments defined in platformio.ini. One per BOARD -- panel
# arrangement is a runtime setting (TA-0303), so a single binary drives 128x32,
# the 2x2 grid and chained 64x64 panels alike. The display-tokenised envs are
# gone; they compiled to firmware that differed only in a value now stored in
# NVS.
#
# Keeping the asset list short still matters: r8 devices parse the release JSON
# unfiltered into an 8KB document that overflows at FOUR assets, so a release
# carrying many variants is invisible to them.
VARIANTS=("matrixportal_s3" "esp32_s3_n8r2")

# Parse arguments
BUILD_VARIANT=""
CLEAN_DIST=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -e|--env)
            BUILD_VARIANT="$2"
            shift 2
            ;;
        -c|--clean)
            CLEAN_DIST=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  -e, --env VARIANT   Build specific variant only, one of:"
            echo "                        matrixportal_s3   Adafruit MatrixPortal ESP32-S3"
            echo "                        esp32_s3_n8r2     Generic ESP32-S3 N8R2 DevKit"
            echo "  -c, --clean         Clean dist/ before building"
            echo "  -h, --help          Show this help"
            echo ""
            echo "Without options, builds all variants."
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Clean dist/ if requested
if [ "$CLEAN_DIST" = true ] && [ -d "dist" ]; then
    echo "Cleaning dist/..."
    rm -f dist/*.bin
fi

# Build
if [ -n "$BUILD_VARIANT" ]; then
    echo "Building SpojBoard firmware for: $BUILD_VARIANT"
    pio run -e "$BUILD_VARIANT"
else
    echo "Building SpojBoard firmware for all variants..."
    for variant in "${VARIANTS[@]}"; do
        echo ""
        echo "━━━ Building: $variant ━━━"
        pio run -e "$variant"
    done
fi

# Show results
echo ""
if [ -d "dist" ] && [ "$(ls -A dist/*.bin 2>/dev/null)" ]; then
    echo "✓ Build complete!"
    echo ""
    echo "Firmware files in dist/:"
    ls -lh dist/*.bin
    echo ""
else
    echo "⚠ Warning: No firmware files found in dist/"
fi
