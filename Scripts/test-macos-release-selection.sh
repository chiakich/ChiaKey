#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_DIR="$(mktemp -d "${TMPDIR:-/tmp}/chiakey-mac-release.XXXXXX")"
trap 'rm -rf "$TEST_DIR"' EXIT
SERVICE_DIR="$ROOT_DIR/ChiaKey-Source/Utilities/Updater/OSX"
clang -fblocks -I"$SERVICE_DIR" \
  "$ROOT_DIR/Scripts/tests/TestMacReleaseSelection.m" \
  "$SERVICE_DIR/ChiaKeyUpdateService.m" \
  -framework Cocoa -o "$TEST_DIR/test-release"
"$TEST_DIR/test-release"
