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
    # field, which is the LAST field, so it cannot be mistaken for any other.
    if env.get("FIRMWARE_BUILD_DIRTY"):
        build_id = f"{build_id}-dirty"

    # Get hardware variant from project environment
    variant = env.GetProjectOption("custom_hardware_variant", "unknown")

    # Optional display-geometry field (TA-0269 SS3). Absent on the 2x32 envs, which
    # keep emitting the legacy bare name.
    display = env.GetProjectOption("custom_display_variant", "")

    # Source firmware path
    firmware_source = str(target[0])

    # Destination, per src/network/OtaAssetName.h:
    #     spojboard-<board>-[<display>-]r<release>-<buildid>[-dirty].bin
    #
    # NO DISPLAY TOKEN MAY BEGIN WITH 'r'. r8's parser reads the board field as
    # the text up to the FIRST "-r", so a token like "rgb" would truncate the
    # board name and let an r8 device accept another board's firmware. r8 is in
    # the field and cannot be fixed, so this constraint is permanent.
    if display.startswith("r"):
        raise ValueError(
            f"custom_display_variant '{display}' begins with 'r' -- this makes r8 "
            f"devices accept firmware built for a different board. Rename it."
        )

    dist_dir = os.path.join(env.get("PROJECT_DIR"), "dist")
    os.makedirs(dist_dir, exist_ok=True)

    display_field = f"{display}-" if display else ""
    firmware_name = f"spojboard-{variant}-{display_field}r{release_num}-{build_id}.bin"
    firmware_dest = os.path.join(dist_dir, firmware_name)

    # Copy firmware
    shutil.copy2(firmware_source, firmware_dest)

    print(f"\n✓ Firmware copied to: dist/{firmware_name}")
    print(f"  Variant: {variant}")
    print(f"  Display: {display or '(bare/legacy)'}")
    print(f"  Release: {release_num}")
    print(f"  Build ID: {build_id}\n")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", post_program_action)
