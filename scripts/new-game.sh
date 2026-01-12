#!/bin/bash
#
# new-game.sh - Create a new Anchor game project
#
# USAGE:
#   ./scripts/new-game.sh <game-name> [--from <previous-game>]
#
# EXAMPLES:
#   ./scripts/new-game.sh my-awesome-game
#   ./scripts/new-game.sh my-new-game --from emoji-ball-battles
#
# This creates:
#   E:/a327ex/my-awesome-game/
#   ├── tools/
#   │   ├── anchor.exe    (copied from Anchor/engine/build/)
#   │   └── yue.exe       (copied from previous game or downloaded)
#   ├── anchor/           (framework: copied from Anchor/game/ or previous game)
#   │   ├── init.yue
#   │   └── object.yue
#   ├── main.yue          (game entry point template)
#   └── assets/           (empty assets folder)
#
# And a private GitHub repo at: github.com/a327ex/my-awesome-game
#
# ============================================================================
# PREREQUISITES (run these once on a new computer)
# ============================================================================
#
# 1. Git with SSH key configured for GitHub
# 2. GitHub CLI (gh) for creating repos: https://cli.github.com/
#
# ============================================================================

set -e  # Exit on any error

# ----------------------------------------------------------------------------
# Configuration
# ----------------------------------------------------------------------------

GAMES_ROOT="E:/a327ex"
ANCHOR_ROOT="E:/a327ex/Anchor"
GITHUB_USER="a327ex"
GH_CMD="/c/Program Files/GitHub CLI/gh.exe"
YUE_URL="https://github.com/IppClub/YueScript/releases/download/v0.30.4/yue-windows-x64.7z"

# ----------------------------------------------------------------------------
# Parse arguments
# ----------------------------------------------------------------------------

GAME_NAME=""
FROM_GAME=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --from)
            FROM_GAME="$2"
            shift 2
            ;;
        *)
            if [ -z "$GAME_NAME" ]; then
                GAME_NAME="$1"
            fi
            shift
            ;;
    esac
done

if [ -z "$GAME_NAME" ]; then
    echo "Usage: $0 <game-name> [--from <previous-game>]"
    echo "Example: $0 my-awesome-game"
    echo "Example: $0 my-new-game --from emoji-ball-battles"
    exit 1
fi

GAME_PATH="$GAMES_ROOT/$GAME_NAME"

# Check if folder already exists
if [ -d "$GAME_PATH" ]; then
    echo "Error: Folder already exists: $GAME_PATH"
    exit 1
fi

# If --from specified, check it exists
if [ -n "$FROM_GAME" ]; then
    FROM_PATH="$GAMES_ROOT/$FROM_GAME"
    if [ ! -d "$FROM_PATH" ]; then
        echo "Error: Source game not found: $FROM_PATH"
        exit 1
    fi
    echo "Creating new game: $GAME_NAME (copying framework from $FROM_GAME)"
else
    echo "Creating new game: $GAME_NAME (using master framework from Anchor/game/)"
fi
echo "Location: $GAME_PATH"
echo ""

# ----------------------------------------------------------------------------
# Step 1: Create game folder structure
# ----------------------------------------------------------------------------

echo "[1/5] Creating game folder..."
mkdir -p "$GAME_PATH/tools"
mkdir -p "$GAME_PATH/anchor"
mkdir -p "$GAME_PATH/assets"
cd "$GAME_PATH"

# ----------------------------------------------------------------------------
# Step 2: Copy tools (anchor.exe, yue.exe)
# ----------------------------------------------------------------------------

echo "[2/5] Copying tools..."

# Copy anchor.exe from Anchor build
if [ -f "$ANCHOR_ROOT/engine/build/anchor.exe" ]; then
    cp "$ANCHOR_ROOT/engine/build/anchor.exe" tools/
else
    echo "Warning: anchor.exe not found at $ANCHOR_ROOT/engine/build/"
    echo "Run build.bat in Anchor/engine/ first."
fi

# Copy yue.exe from previous game or download
if [ -n "$FROM_GAME" ] && [ -f "$FROM_PATH/tools/yue.exe" ]; then
    cp "$FROM_PATH/tools/yue.exe" tools/
elif [ -f "$GAMES_ROOT/emoji-ball-battles/tools/yue.exe" ]; then
    # Fallback: try emoji-ball-battles
    cp "$GAMES_ROOT/emoji-ball-battles/tools/yue.exe" tools/
else
    echo "Downloading yue.exe..."
    curl -L -o tools/yue-windows-x64.7z "$YUE_URL"
    "/c/Program Files/7-Zip/7z.exe" x tools/yue-windows-x64.7z -otools -y
    rm tools/yue-windows-x64.7z
fi

# ----------------------------------------------------------------------------
# Step 3: Copy framework (anchor/*.yue)
# ----------------------------------------------------------------------------

echo "[3/5] Copying framework..."

if [ -n "$FROM_GAME" ]; then
    # Copy from previous game
    cp "$FROM_PATH/anchor/"*.yue anchor/
else
    # Copy from Anchor/game/ (master copy)
    cp "$ANCHOR_ROOT/game/"*.yue anchor/
fi

# ----------------------------------------------------------------------------
# Step 4: Create template files
# ----------------------------------------------------------------------------

echo "[4/5] Creating template files..."

# main.yue - Game entry point template
cat > main.yue << 'MAIN_EOF'
require 'anchor'

-- Game initialization here

MAIN_EOF

# .gitkeep for assets
touch assets/.gitkeep

# .gitignore
cat > .gitignore << 'GITIGNORE_EOF'
# Compiled Lua (generated from .yue)
*.lua

# Build artifacts
*.obj
*.o

# Editor files
*.swp
*.swo
*~
.vscode/
.idea/

# OS files
.DS_Store
Thumbs.db

# Temp files
tmpclaude-*
GITIGNORE_EOF

# ----------------------------------------------------------------------------
# Step 5: Initialize git and create GitHub repo
# ----------------------------------------------------------------------------

echo "[5/5] Creating git repo..."

git init
git branch -M main

# Check if gh is available
if [ -f "$GH_CMD" ]; then
    "$GH_CMD" repo create "$GITHUB_USER/$GAME_NAME" --private --source=. --remote=origin

    git add -A
    git commit -m "Initial commit: Anchor game scaffold

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"

    git push -u origin main

    echo ""
    echo "GitHub repo created: https://github.com/$GITHUB_USER/$GAME_NAME"
else
    echo "GitHub CLI not found, skipping repo creation."
    echo "You can create the repo manually later."
fi

# ----------------------------------------------------------------------------
# Done!
# ----------------------------------------------------------------------------

echo ""
echo "=========================================="
echo "Game created successfully!"
echo "=========================================="
echo ""
echo "Location: $GAME_PATH"
echo ""
echo "Next steps:"
echo "  cd $GAME_PATH"
echo "  # Edit main.yue"
echo "  tools/yue.exe -r anchor        # Compile framework"
echo "  tools/yue.exe -r main.yue      # Compile game"
echo "  tools/anchor.exe .             # Run game"
echo ""
