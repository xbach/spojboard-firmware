#!/bin/bash
# Prepare a changelog entry for the next release using Claude
# Usage: ./tools/prepare_changelog.sh [release_number]
# If release_number is omitted, auto-detects from CHANGELOG.md

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CHANGELOG="$REPO_DIR/CHANGELOG.md"

if [ ! -f "$CHANGELOG" ]; then
  echo "ERROR: CHANGELOG.md not found at $CHANGELOG"
  exit 1
fi

# Determine release number
if [ -n "$1" ]; then
  RELEASE_NUM=$1
else
  # Auto-detect: find highest rN in CHANGELOG.md, increment by 1
  LAST_RELEASE=$(grep -o '## \[r[0-9]*\]' "$CHANGELOG" | head -1 | sed 's/.*r\([0-9]*\).*/\1/')
  if [ -z "$LAST_RELEASE" ]; then
    echo "ERROR: Could not determine last release number from CHANGELOG.md"
    exit 1
  fi
  RELEASE_NUM=$((LAST_RELEASE + 1))
fi

TAG_NAME="r${RELEASE_NUM}"

# Check if entry already exists
if grep -q "^## \[r${RELEASE_NUM}\]" "$CHANGELOG"; then
  echo "Changelog entry for [r${RELEASE_NUM}] already exists."
  exit 0
fi

# Find the last tag for git log range
LAST_TAG=$(git -C "$REPO_DIR" describe --tags --abbrev=0 2>/dev/null || echo "")
if [ -z "$LAST_TAG" ]; then
  echo "ERROR: No previous tags found. Cannot determine commit range."
  exit 1
fi

echo "Preparing changelog for r${RELEASE_NUM} (commits since ${LAST_TAG})..."
echo ""

# Get git log since last tag
GIT_LOG=$(git -C "$REPO_DIR" log --oneline "${LAST_TAG}..HEAD")

if [ -z "$GIT_LOG" ]; then
  echo "ERROR: No commits found since ${LAST_TAG}."
  exit 1
fi

echo "=== Commits since ${LAST_TAG} ==="
echo "$GIT_LOG"
echo ""

# Use Claude to generate the changelog entry
PROMPT="Generate a changelog entry for release r${RELEASE_NUM} of SpojBoard firmware.

Here are the commits since the last release (${LAST_TAG}):

${GIT_LOG}

Format rules:
- Use Keep a Changelog style with ### Added, ### Changed, ### Fixed, ### Performance sections (only include sections that apply)
- Each item is a single line starting with \`- \`
- Be concise but descriptive — summarize what changed, not how
- Group related commits into single entries when appropriate
- Do NOT include the ## [rN] header line — I will add that myself
- Do NOT include chore/meta commits (changelog updates, version bumps, CI changes)
- Do NOT include any commentary, insight blocks, callouts, horizontal rules, or markdown beyond the ### section headers and bullet lines
- Output ONLY the changelog sections, nothing else — no intro, no explanation, no closing remarks"

ENTRY=$(claude -p "$PROMPT" --max-turns 1 2>/dev/null)

# Strip any leaked Insight blocks (explanatory output style emits these despite prompt)
# Matches an opening "★ Insight ─────" line through the next "─────" closing line,
# then collapses runs of blank lines that result.
ENTRY=$(echo "$ENTRY" | awk '
  /^`?★ Insight/ { in_insight=1; next }
  in_insight && /^`?─+`?$/ { in_insight=0; next }
  !in_insight {
    if ($0 == "") { if (!blank) print; blank=1 }
    else { print; blank=0 }
  }
')

if [ -z "$ENTRY" ]; then
  echo "ERROR: Claude returned empty response."
  exit 1
fi

echo "=== Generated changelog entry ==="
echo ""
echo "$ENTRY"
echo ""

# Ask for confirmation
read -p "Add this entry to CHANGELOG.md as [r${RELEASE_NUM}]? (y/e/N) " -n 1 -r
echo
if [[ $REPLY =~ ^[Ee]$ ]]; then
  # Open in editor for manual tweaks
  TMPFILE=$(mktemp)
  echo "$ENTRY" > "$TMPFILE"
  ${EDITOR:-vi} "$TMPFILE"
  ENTRY=$(cat "$TMPFILE")
  rm "$TMPFILE"
elif [[ ! $REPLY =~ ^[Yy]$ ]]; then
  echo "Aborted."
  exit 1
fi

# Write new section to a temp file (awk -v can't accept multi-line values)
SECTION_FILE=$(mktemp)
{
  echo "## [r${RELEASE_NUM}] - Unreleased"
  echo
  echo "$ENTRY"
  echo
} > "$SECTION_FILE"

# Insert the section before the first existing "## [rN]" line
awk -v sf="$SECTION_FILE" '
  /^## \[r[0-9]/ && !inserted {
    while ((getline line < sf) > 0) print line
    close(sf)
    inserted=1
  }
  { print }
' "$CHANGELOG" > "${CHANGELOG}.tmp" && mv "${CHANGELOG}.tmp" "$CHANGELOG"
rm "$SECTION_FILE"

# Verify the entry actually landed (awk can exit 0 even after stderr errors)
if ! grep -q "^## \[r${RELEASE_NUM}\]" "$CHANGELOG"; then
  echo "ERROR: Insertion failed — [r${RELEASE_NUM}] not found in CHANGELOG.md"
  exit 1
fi

echo ""
echo "Changelog entry for [r${RELEASE_NUM}] added to CHANGELOG.md"
echo "Review and edit if needed before releasing."
