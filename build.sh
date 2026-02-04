#!/bin/bash
# Build SpojBoard firmware locally
# Supports multiple hardware variants

set -e

# Hardware variants defined in platformio.ini
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
            echo "  -e, --env VARIANT   Build specific variant only (matrixportal_s3 or esp32_s3_n8r2)"
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
