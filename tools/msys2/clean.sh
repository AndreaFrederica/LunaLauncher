#!/usr/bin/env bash
# tools/msys2/clean.sh - Clean build directories

set -e

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

echo ">> Removing build and install directories"
rm -rf "$REPO_DIR/build" "$REPO_DIR/install"

echo ">> Done"
