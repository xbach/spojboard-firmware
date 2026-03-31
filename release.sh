#!/bin/bash
# SpojBoard Release Helper Script
# Usage: ./release.sh <release_number>
# Example: ./release.sh 3

set -e

if [ -z "$1" ]; then
  echo "Usage: ./release.sh <release_number>"
  echo "Example: ./release.sh 3"
  exit 1
fi

RELEASE_NUM=$1
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

# Check if working directory is clean
if [ -n "$(git status --porcelain)" ]; then
  echo "WARNING: Working directory is not clean!"
  echo ""
  git status --short
  echo ""
  read -p "Continue anyway? (y/N) " -n 1 -r
  echo
  if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Aborted."
    exit 1
  fi
fi

# Check if CHANGELOG.md has entry for this release
if ! grep -q "## \[r${RELEASE_NUM}\]" CHANGELOG.md; then
  echo "WARNING: No changelog entry found for [r${RELEASE_NUM}] in CHANGELOG.md"
  echo ""
  read -p "Generate changelog entry with Claude? (y/N) " -n 1 -r
  echo
  if [[ $REPLY =~ ^[Yy]$ ]]; then
    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
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

read -p "Create and push tag ${TAG_NAME}? (y/N) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
  echo "Aborted."
  exit 1
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
