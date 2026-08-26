#!/bin/bash
# Build SpojBoard firmware locally
# Supports multiple hardware variants

set -e

# Build environments defined in platformio.ini.
#
# "matrixportal_s3" and "esp32_s3_n8r2" are the 2x32 builds and emit the BARE
# asset name; the two suffixed envs emit display-tokenised names (TA-0269 SS3).
#
# Which of these a RELEASE actually publishes is a separate decision from which
# ones build here: r8 devices parse the release JSON unfiltered into an 8KB
# document that overflows at FOUR assets, so a release carrying every variant is
# invisible to them. See TA-0269 "Left" item 1.
VARIANTS=("matrixportal_s3" "matrixportal_s3_4x32" "matrixportal_s3_2x64" "esp32_s3_n8r2")

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
            echo "                        matrixportal_s3        128x32, 2x 64x32 (bare asset name)"
            echo "                        matrixportal_s3_4x32   128x64, 4x 64x32"
            echo "                        matrixportal_s3_2x64   128x64, 2x 64x64"
            echo "                        esp32_s3_n8r2          128x32, 2x 64x32 (bare asset name)"
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
