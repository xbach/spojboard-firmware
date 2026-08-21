Import("env")
import shutil
import os
import re

def post_program_action(source, target, env):
    """Copy firmware.bin to dist/ with version naming including hardware variant"""

    # Extract FIRMWARE_RELEASE from AppConfig.h
    config_path = os.path.join(env.get("PROJECT_DIR"), "src/config/AppConfig.h")
    release_num = "unknown"

    with open(config_path, 'r') as f:
        content = f.read()
        match = re.search(r'#define\s+FIRMWARE_RELEASE\s+"([^"]+)"', content)
        if match:
            release_num = match.group(1)

    # Get build ID from environment (set by build_id.py)
    build_id = env.get("FIRMWARE_BUILD_ID", "unknown")

    # A dirty build is NOT the commit its ID names, so it must not be able to
    # masquerade as a release artifact in dist/. The suffix goes on the build-ID
    # field, after the "-r<release>-" marker, so the variant is still the text
    # between "spojboard-" and the first "-r" -- the grammar GitHubOTA.cpp parses.
    if env.get("FIRMWARE_BUILD_DIRTY"):
        build_id = f"{build_id}-dirty"

    # Get hardware variant from project environment
    variant = env.GetProjectOption("custom_hardware_variant", "unknown")

    # Source firmware path
    firmware_source = str(target[0])

    # Destination: dist/spojboard-{variant}-r{release}-{build}.bin
    dist_dir = os.path.join(env.get("PROJECT_DIR"), "dist")
    os.makedirs(dist_dir, exist_ok=True)

    firmware_name = f"spojboard-{variant}-r{release_num}-{build_id}.bin"
    firmware_dest = os.path.join(dist_dir, firmware_name)

    # Copy firmware
    shutil.copy2(firmware_source, firmware_dest)

    print(f"\n✓ Firmware copied to: dist/{firmware_name}")
    print(f"  Variant: {variant}")
    print(f"  Release: {release_num}")
    print(f"  Build ID: {build_id}\n")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", post_program_action)
