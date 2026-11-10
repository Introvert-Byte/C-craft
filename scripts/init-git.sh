#!/usr/bin/env bash
#
# init-git.sh - Initialize Git, link the GitHub remote (using the provided
#               Personal Access Token), stage, commit and push C+craft.
#
# USAGE
#   1. Edit the USERNAME and REPOSITORY_NAME variables below.
#   2. chmod +x scripts/init-git.sh
#   3. ./scripts/init-git.sh
#
# The GitHub PAT is embedded in the remote URL (no separate credential helper
# required):
#   https://<GITHUB_PAT>@github.com/USERNAME/REPOSITORY_NAME.git
#   Set GITHUB_PAT env var, or replace the placeholder below with your PAT.
#
set -euo pipefail

# ---------------------------------------------------------------------------
# CONFIGURE THESE BEFORE RUNNING
# ---------------------------------------------------------------------------
USERNAME="${GIT_USERNAME:-USERNAME}"          # <-- replace with your GitHub username
REPOSITORY_NAME="${GIT_REPO:-REPOSITORY_NAME}" # <-- replace with your repo name
GITHUB_PAT="${GITHUB_PAT:-<YOUR_GITHUB_PAT>}"  # <-- replace with your PAT (or export GITHUB_PAT)
# ---------------------------------------------------------------------------

REMOTE_URL="https://${GITHUB_PAT}@github.com/${USERNAME}/${REPOSITORY_NAME}.git"

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_DIR"

echo "[1/6] Verifying placeholders are replaced..."
if [ "$USERNAME" = "USERNAME" ] || [ "$REPOSITORY_NAME" = "REPOSITORY_NAME" ]; then
    echo "ERROR: Please set USERNAME and REPOSITORY_NAME in this script (or via"
    echo "       GIT_USERNAME / GIT_REPO environment variables) before running."
    exit 1
fi

echo "[2/6] Initializing Git repository..."
git init

echo "[3/6] Configuring Git identity..."
git config user.name "${GIT_AUTHOR_NAME:-C+craft Author}"
git config user.email "${GIT_AUTHOR_EMAIL:-craft@local.engine}"

echo "[4/6] Staging initial source files and creating commit..."
git add -A
git commit -m "Initial commit: Voxel Engine core foundation"

echo "[5/6] Linking GitHub remote with the provided PAT..."
if git remote get-url origin >/dev/null 2>&1; then
    git remote set-url origin "$REMOTE_URL"
else
    git remote add origin "$REMOTE_URL"
fi

echo "[6/6] Pushing to GitHub..."
git branch -M main
git push -u origin main

echo ""
echo "Done! Repository pushed to: $REMOTE_URL"
