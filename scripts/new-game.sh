#!/bin/bash
#
# new-game.sh - Create a new Anchor game project
#
# This script creates a new game folder with Anchor as a submodule,
# sets up the yue junction for framework access, creates a private
# GitHub repo, and pushes the initial commit.
#
# USAGE:
#   ./scripts/new-game.sh <game-name>
#
# EXAMPLE:
#   ./scripts/new-game.sh my-awesome-game
#
# This creates:
#   E:/a327ex/my-awesome-game/
#   ├── anchor/          (submodule → Anchor repo)
#   ├── yue/             (junction → anchor/engine/yue)
#   ├── main.yue         (game entry point template)
#   ├── assets/          (empty assets folder)
#   └── setup.bat        (for other devs after cloning)
#
# And a private GitHub repo at: github.com/a327ex/my-awesome-game
#
# ============================================================================
# PREREQUISITES (run these once on a new computer)
# ============================================================================
#
# 1. Git with SSH key configured for GitHub
#    - Generate key: ssh-keygen -t ed25519 -C "your_email@example.com"
#    - Add to agent: eval "$(ssh-agent -s)" && ssh-add ~/.ssh/id_ed25519
#    - Add public key to GitHub: Settings → SSH and GPG keys → New SSH key
#    - Test: ssh -T git@github.com
#
# 2. GitHub CLI (gh) for creating repos programmatically
#    - Download: https://cli.github.com/
#    - After install, authenticate: gh auth login
#      - Select: GitHub.com
#      - Select: SSH
#      - Select your SSH key
#      - Select: Login with a web browser (or paste token)
#
# ============================================================================

set -e  # Exit on any error

# ----------------------------------------------------------------------------
# Configuration
# ----------------------------------------------------------------------------

GAMES_ROOT="E:/a327ex"                          # Where game folders live
ANCHOR_REPO="git@github.com:a327ex/Anchor.git"  # Anchor repo (SSH)
GITHUB_USER="a327ex"                            # GitHub username for new repos
GH_CMD="/c/Program Files/GitHub CLI/gh.exe"     # GitHub CLI path on Windows

# ----------------------------------------------------------------------------
# Validate arguments
# ----------------------------------------------------------------------------

if [ -z "$1" ]; then
    echo "Usage: $0 <game-name>"
    echo "Example: $0 my-awesome-game"
    exit 1
fi

GAME_NAME="$1"
GAME_PATH="$GAMES_ROOT/$GAME_NAME"

# Check if folder already exists
if [ -d "$GAME_PATH" ]; then
    echo "Error: Folder already exists: $GAME_PATH"
    exit 1
fi

# Check if gh is available
if [ ! -f "$GH_CMD" ]; then
    echo "Error: GitHub CLI not found at: $GH_CMD"
    echo "Install from: https://cli.github.com/"
    echo "Then run: gh auth login"
    exit 1
fi

echo "Creating new Anchor game: $GAME_NAME"
echo "Location: $GAME_PATH"
echo ""

# ----------------------------------------------------------------------------
# Step 1: Create game folder and initialize git
# ----------------------------------------------------------------------------

echo "[1/6] Creating game folder..."
mkdir -p "$GAME_PATH"
cd "$GAME_PATH"

echo "[2/6] Initializing git repository..."
git init
git branch -M main  # Ensure main branch (not master)

# ----------------------------------------------------------------------------
# Step 2: Add Anchor as submodule
# ----------------------------------------------------------------------------

echo "[3/6] Adding Anchor as submodule..."
# This clones Anchor into the 'anchor' folder and sets up .gitmodules
git submodule add "$ANCHOR_REPO" anchor

# ----------------------------------------------------------------------------
# Step 3: Create yue junction (Windows directory symlink alternative)
# ----------------------------------------------------------------------------

echo "[4/6] Creating yue junction..."
# Junctions work without admin rights on Windows (unlike symlinks)
# This allows: require 'yue.object' to find anchor/engine/yue/object.lua
cmd //c "mklink /J yue anchor\\engine\\yue"

# ----------------------------------------------------------------------------
# Step 4: Create template files
# ----------------------------------------------------------------------------

echo "[5/6] Creating template files..."

# setup.bat - For other developers after cloning
cat > setup.bat << 'SETUP_EOF'
@echo off
REM Setup script for this game
REM Run this after cloning: git clone --recursive <repo>

REM Initialize and update submodule (in case --recursive was forgotten)
git submodule update --init

REM Create junction to framework (works without admin on Windows)
if not exist yue (
    mklink /J yue anchor\engine\yue
    echo Junction created: yue -^> anchor\engine\yue
) else (
    echo yue junction already exists
)

echo Setup complete!
SETUP_EOF

# main.yue - Game entry point template
cat > main.yue << 'MAIN_EOF'
-- Main entry point for the game
-- This file is compiled to main.lua by YueScript

-- Framework imports
-- object = require 'yue.object'
-- timer = require 'yue.timer'

-- Called once at startup
export init = ->
  print "Game initialized!"

-- Called every frame (dt = delta time in seconds)
export update = (dt) ->
  nil

-- Called every frame for rendering
export draw = ->
  nil
MAIN_EOF

# Create empty assets folder with .gitkeep
mkdir -p assets
touch assets/.gitkeep

# .gitignore for the game
cat > .gitignore << 'GITIGNORE_EOF'
# Compiled Lua (generated from .yue)
*.lua

# Build artifacts
*.exe
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
GITIGNORE_EOF

# ----------------------------------------------------------------------------
# Step 5: Create GitHub repo and push
# ----------------------------------------------------------------------------

echo "[6/6] Creating private GitHub repo and pushing..."

# Create private repo on GitHub using gh CLI
"$GH_CMD" repo create "$GITHUB_USER/$GAME_NAME" --private --source=. --remote=origin

# Stage all files
git add -A

# Initial commit
git commit -m "Initial commit: Anchor game scaffold

- Add Anchor engine as submodule
- Add setup.bat for yue junction creation
- Add main.yue template
- Add assets folder

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"

# Push to GitHub
git push -u origin main

# ----------------------------------------------------------------------------
# Done!
# ----------------------------------------------------------------------------

echo ""
echo "=========================================="
echo "Game created successfully!"
echo "=========================================="
echo ""
echo "Location: $GAME_PATH"
echo "GitHub:   https://github.com/$GITHUB_USER/$GAME_NAME"
echo ""
echo "Next steps:"
echo "  cd $GAME_PATH"
echo "  # Edit main.yue"
echo "  # Run with: ./anchor/engine/build/anchor.exe ."
echo ""
