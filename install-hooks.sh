#!/bin/bash

# Install git hooks for I2C A78 Driver project

set -e

# Get the repository root
REPO_ROOT=$(git rev-parse --show-toplevel 2>/dev/null)
if [ $? -ne 0 ]; then
    echo "Error: Not in a git repository"
    exit 1
fi

echo "Installing git hooks for I2C A78 Driver project..."

# Create .git/hooks directory if it doesn't exist
mkdir -p "$REPO_ROOT/.git/hooks"

# Install pre-commit hook
if [ -f "$REPO_ROOT/.githooks/pre-commit" ]; then
    cp "$REPO_ROOT/.githooks/pre-commit" "$REPO_ROOT/.git/hooks/pre-commit"
    chmod +x "$REPO_ROOT/.git/hooks/pre-commit"
    echo "✅ Pre-commit hook installed"
else
    echo "⚠️  Pre-commit hook not found in .githooks/"
fi

# Set up git hooks path (optional, for shared hooks)
git config core.hooksPath .githooks

echo "Git hooks installation complete!"
echo ""
echo "The following hooks are now active:"
echo "  - pre-commit: Runs tests and style checks before each commit"
echo ""
echo "To bypass hooks temporarily, use: git commit --no-verify"