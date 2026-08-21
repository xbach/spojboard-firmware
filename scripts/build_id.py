Import("env")
import subprocess

# BUILD_ID identifies the SOURCE a binary was built from.
#
# It is the first 8 hex chars of the git HEAD SHA, used verbatim rather than
# hashed: BUILD_ID is a uint32 printed as "%08x", and a SHA prefix is already
# exactly 8 hex chars. So the ID a device shows on its System tab is a real git
# reference -- read "363852da" off a panel and `git show 363852da` lands on the
# exact source it runs.
#
# This replaced a datetime.now()-derived hash, which made every build of the same
# commit different and a dirty tree indistinguishable from a release build. That
# matters more here than almost anywhere: SpojBoard ships GitHub OTA, so an
# untraceable binary is an untraceable binary already installed on someone else's
# hardware.

PROJECT_DIR = env.get("PROJECT_DIR")


def git(*args):
    """Run a git command in the project dir. Returns stripped stdout, or None on failure."""
    try:
        out = subprocess.check_output(
            ["git", "-C", PROJECT_DIR, *args],
            stderr=subprocess.DEVNULL,
        )
        return out.decode().strip()
    except Exception:
        return None


sha = git("rev-parse", "HEAD")

if sha is None:
    # No git checkout: the binary's provenance is genuinely unknown. Fall back to
    # the same all-zero value AppConfig.h uses for "build system didn't set this",
    # and say so loudly. Deliberately NOT falling back to a timestamp -- that would
    # silently restore the nondeterminism this script exists to remove.
    build_id = "00000000"
    dirty = True
    print("")
    print("*** WARNING: not a git checkout (or git unavailable).")
    print("*** BUILD_ID=00000000 -- this binary's source is UNIDENTIFIABLE. Do not ship it.")
    print("")
else:
    build_id = sha[:8]
    status = git("status", "--porcelain")
    # status is "" when clean. None means the command failed -> assume dirty.
    dirty = (status is None) or (status != "")
    if dirty:
        print("")
        print(f"*** WARNING: working tree is dirty. BUILD_ID {build_id} names commit {build_id},")
        print("*** but this binary does NOT match that commit. Commit before shipping.")
        print("")

env.Append(
    CPPDEFINES=[
        ("BUILD_ID", f"0x{build_id}"),
        ("BUILD_DIRTY", "1" if dirty else "0"),
    ]
)

# Consumed by post_build.py for the dist/ filename
env["FIRMWARE_BUILD_ID"] = build_id
env["FIRMWARE_BUILD_DIRTY"] = dirty
