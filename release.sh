#!/bin/bash
# SpojBoard Release Helper Script
# Usage: ./release.sh [--autogen-changelog] <release_number>
# Example: ./release.sh 3
#          ./release.sh --autogen-changelog 3

set -e

usage() {
  echo "Usage: ./release.sh [options] <release_number>"
  echo "Example: ./release.sh 3"
  echo "         ./release.sh --yes 3"
  echo ""
  echo "Options:"
  echo "  -y, --yes, --non-interactive"
  echo "        Run unattended: assume \"yes\" to every prompt (dirty tree, tag"
  echo "        create/push) and auto-generate the changelog if it is missing."
  echo "  --autogen-changelog"
  echo "        Generate the [rN] changelog entry non-interactively (via"
  echo "        tools/prepare_changelog.sh --yes). Fails if an entry for [rN]"
  echo "        already exists. Combine with --yes for a fully unattended,"
  echo "        force-fresh-changelog release."
}

AUTOGEN_CHANGELOG=false
ASSUME_YES=false
RELEASE_NUM=""
for arg in "$@"; do
  case "$arg" in
    --autogen-changelog) AUTOGEN_CHANGELOG=true ;;
    -y|--yes|--non-interactive) ASSUME_YES=true ;;
    -h|--help) usage; exit 0 ;;
    -*) echo "ERROR: Unknown option: $arg"; echo ""; usage; exit 1 ;;
    *)
      if [ -n "$RELEASE_NUM" ]; then
        echo "ERROR: Multiple release numbers given ('$RELEASE_NUM' and '$arg')"
        exit 1
      fi
      RELEASE_NUM="$arg"
      ;;
  esac
done

if [ -z "$RELEASE_NUM" ]; then
  usage
  exit 1
fi

TAG_NAME="r${RELEASE_NUM}"

echo "=== SpojBoard Release Helper ==="
echo "Release number: ${RELEASE_NUM}"
echo "Tag name: ${TAG_NAME}"
echo ""

# Check if tag already exists
if git rev-parse "$TAG_NAME" >/dev/null 2>&1; then
  echo "ERROR: Tag ${TAG_NAME} already exists!"
  echo "To delete it locally: git tag -d ${TAG_NAME}"
  echo "To delete it remotely: git push origin :refs/tags/${TAG_NAME}"
  exit 1
fi

# Verify FIRMWARE_RELEASE in source matches the release number being tagged
APPCONFIG="src/config/AppConfig.h"
SOURCE_VERSION=$(grep -E '^#define[[:space:]]+FIRMWARE_RELEASE[[:space:]]+"' "$APPCONFIG" | sed -E 's/.*"([^"]+)".*/\1/')
if [ -z "$SOURCE_VERSION" ]; then
  echo "ERROR: Could not read FIRMWARE_RELEASE from ${APPCONFIG}"
  exit 1
fi
if [ "$SOURCE_VERSION" != "$RELEASE_NUM" ]; then
  echo "ERROR: FIRMWARE_RELEASE in ${APPCONFIG} is \"${SOURCE_VERSION}\" but you're tagging r${RELEASE_NUM}."
  echo "Binaries would self-report as r${SOURCE_VERSION}, breaking GitHub OTA version checks."
  echo ""
  echo "Fix:"
  echo "  sed -i '' 's/FIRMWARE_RELEASE \"${SOURCE_VERSION}\"/FIRMWARE_RELEASE \"${RELEASE_NUM}\"/' ${APPCONFIG}"
  echo "  git commit -am \"chore: bump FIRMWARE_RELEASE to ${RELEASE_NUM}\""
  exit 1
fi

# Check if working directory is clean
if [ -n "$(git status --porcelain)" ]; then
  echo "WARNING: Working directory is not clean!"
  echo ""
  git status --short
  echo ""
  if [ "$ASSUME_YES" = true ]; then
    echo "Proceeding despite dirty working tree (--yes)."
  else
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
      echo "Aborted."
      exit 1
    fi
  fi
fi

# Changelog handling
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CHANGELOG_EXISTS=false
if grep -q "## \[r${RELEASE_NUM}\]" CHANGELOG.md; then
  CHANGELOG_EXISTS=true
fi

if [ "$AUTOGEN_CHANGELOG" = true ]; then
  # Non-interactive generation. Fail if an entry already exists so we never
  # silently release stale/hand-written notes when fresh ones were requested.
  if [ "$CHANGELOG_EXISTS" = true ]; then
    echo "ERROR: --autogen-changelog was given but a changelog entry for [r${RELEASE_NUM}] already exists."
    echo "Remove the existing [r${RELEASE_NUM}] section first, or drop --autogen-changelog to use it as-is."
    exit 1
  fi
  echo "Auto-generating changelog entry for [r${RELEASE_NUM}]..."
  "${SCRIPT_DIR}/tools/prepare_changelog.sh" --yes "${RELEASE_NUM}"
  # Re-check after generation
  if ! grep -q "## \[r${RELEASE_NUM}\]" CHANGELOG.md; then
    echo "ERROR: Changelog entry still missing after generation."
    exit 1
  fi
elif [ "$CHANGELOG_EXISTS" = false ]; then
  if [ "$ASSUME_YES" = true ]; then
    echo "No changelog entry for [r${RELEASE_NUM}]; auto-generating (--yes)..."
    "${SCRIPT_DIR}/tools/prepare_changelog.sh" --yes "${RELEASE_NUM}"
    if ! grep -q "## \[r${RELEASE_NUM}\]" CHANGELOG.md; then
      echo "ERROR: Changelog entry still missing after generation."
      exit 1
    fi
  else
    echo "WARNING: No changelog entry found for [r${RELEASE_NUM}] in CHANGELOG.md"
    echo ""
    read -p "Generate changelog entry with Claude? (y/N) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
      "${SCRIPT_DIR}/tools/prepare_changelog.sh" "${RELEASE_NUM}"
      # Re-check after generation
      if ! grep -q "## \[r${RELEASE_NUM}\]" CHANGELOG.md; then
        echo "ERROR: Changelog entry still missing after generation."
        exit 1
      fi
    else
      read -p "Continue without changelog? (y/N) " -n 1 -r
      echo
      if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Aborted."
        exit 1
      fi
    fi
  fi
fi

# Show what will be in the release
echo ""
echo "=== Commits since last release ==="
LAST_TAG=$(git describe --tags --abbrev=0 2>/dev/null || echo "")
if [ -n "$LAST_TAG" ]; then
  git log --oneline ${LAST_TAG}..HEAD
else
  echo "No previous tags found. This will be the first release."
  git log --oneline HEAD~10..HEAD
fi

echo ""
echo "=== Changelog for r${RELEASE_NUM} ==="
awk '/^## \[r'"${RELEASE_NUM}"'\]/{f=1;next} f && /^## \[r[0-9]/{exit} f' CHANGELOG.md | head -20
echo ""

if [ "$ASSUME_YES" = true ]; then
  echo "Creating and pushing tag ${TAG_NAME} (--yes)..."
else
  read -p "Create and push tag ${TAG_NAME}? (y/N) " -n 1 -r
  echo
  if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Aborted."
    exit 1
  fi
fi

# Update CHANGELOG date if it says "Unreleased"
if grep -q "## \[r${RELEASE_NUM}\] - Unreleased" CHANGELOG.md; then
  TODAY=$(date +%Y-%m-%d)
  sed -i.bak "s/## \[r${RELEASE_NUM}\] - Unreleased/## [r${RELEASE_NUM}] - ${TODAY}/" CHANGELOG.md
  rm CHANGELOG.md.bak
  git add CHANGELOG.md
  git commit -m "chore: update CHANGELOG for release r${RELEASE_NUM}" || true
  echo "Updated CHANGELOG.md with release date: ${TODAY}"
fi

# Create and push tag
git tag -a "${TAG_NAME}" -m "Release ${RELEASE_NUM}"
git push origin "${TAG_NAME}"

echo ""
echo "=== Success! ==="
echo "Tag ${TAG_NAME} created and pushed."
echo "GitHub Actions will now:"
echo "  1. Build the firmware"
echo "  2. Create a GitHub release"
echo "  3. Upload the .bin file"
echo "  4. Add changelog from CHANGELOG.md"
echo ""
echo "Monitor progress at:"
echo "https://github.com/xbach/spojboard-firmware/actions"
echo ""
echo "Release will be available at:"
echo "https://github.com/xbach/spojboard-firmware/releases/tag/${TAG_NAME}"
