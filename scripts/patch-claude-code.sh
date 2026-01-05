#!/bin/bash
# Patches Claude Code cli.js to fix the "File has been unexpectedly modified" bug on Windows
# Run this after every Claude Code update: ./scripts/patch-claude-code.sh

# Handle Windows username (try multiple env vars)
WIN_USER="${USERNAME:-${USER:-$(whoami)}}"
CLAUDE_CLI="/c/Users/$WIN_USER/AppData/Roaming/npm/node_modules/@anthropic-ai/claude-code/cli.js"

if [ ! -f "$CLAUDE_CLI" ]; then
    echo "Error: cli.js not found at $CLAUDE_CLI"
    exit 1
fi

# Check current version
VERSION=$(claude --version 2>/dev/null | head -1)
echo "Claude Code version: $VERSION"

# Check if already patched
PATCHED_COUNT=$(grep -c 'if(false)throw Error("File has been unexpectedly modified' "$CLAUDE_CLI" 2>/dev/null || echo "0")
if [ "$PATCHED_COUNT" -ge 2 ]; then
    echo "Already patched ($PATCHED_COUNT occurrences). Nothing to do."
    exit 0
fi

# Find and count the patterns we need to patch
echo "Searching for patterns to patch..."
PATTERNS=$(grep -oE 'if\(![a-zA-Z]\|\|[A-Z]>[a-zA-Z]\.timestamp\)throw Error\("File has been unexpectedly modified' "$CLAUDE_CLI" | sort -u)

if [ -z "$PATTERNS" ]; then
    echo "Warning: No matching patterns found. The cli.js format may have changed."
    echo "Check manually: grep 'unexpectedly modified' \"$CLAUDE_CLI\""
    exit 1
fi

echo "Found patterns:"
echo "$PATTERNS"

# Backup
BACKUP="${CLAUDE_CLI}.backup.$(date +%Y%m%d%H%M%S)"
cp "$CLAUDE_CLI" "$BACKUP"
echo "Backup created: $BACKUP"

# Apply patches - match any single letter variable names
sed -i -E 's/if\(![a-zA-Z]\|\|[A-Z]>[a-zA-Z]\.timestamp\)throw Error\("File has been unexpectedly modified/if(false)throw Error("File has been unexpectedly modified/g' "$CLAUDE_CLI"

# Verify
NEW_PATCHED=$(grep -c 'if(false)throw Error("File has been unexpectedly modified' "$CLAUDE_CLI" 2>/dev/null || echo "0")
REMAINING=$(grep -c 'timestamp)throw Error("File has been unexpectedly modified' "$CLAUDE_CLI" 2>/dev/null || echo "0")

echo ""
echo "=== Results ==="
echo "Patched occurrences: $NEW_PATCHED"
echo "Remaining unpatched: $REMAINING"

if [ "$NEW_PATCHED" -ge 1 ] && [ "$REMAINING" -eq 0 ]; then
    echo "Success! Restart Claude Code for changes to take effect."
else
    echo "Warning: Patch may have been incomplete. Check manually."
fi
