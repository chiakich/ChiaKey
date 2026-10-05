#!/usr/bin/env bash
# Exercise the four distinct updater implementations actually shipped in v1.2.x.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_DIR="$(mktemp -d "${TMPDIR:-/tmp}/chiakey-legacy-mac.XXXXXX")"
trap 'rm -rf "$TEST_DIR"' EXIT
SERVICE=ChiaKey-Source/Utilities/Updater/OSX/ChiaKeyUpdateService.m
for tag in v1.2.0 v1.2.1 v1.2.2 v1.2.6; do
  git -C "$ROOT_DIR" show "$tag:$SERVICE" > "$TEST_DIR/service.m"
  git -C "$ROOT_DIR" show "$tag:${SERVICE%.m}.h" > "$TEST_DIR/ChiaKeyUpdateService.h"
  clang -fblocks -I"$TEST_DIR" \
    "$ROOT_DIR/Scripts/tests/TestLegacyMacReleaseSelection.m" "$TEST_DIR/service.m" \
    -framework Cocoa -o "$TEST_DIR/check"
  printf '%s: ' "$tag"
  "$TEST_DIR/check"
done
