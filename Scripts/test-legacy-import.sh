#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="$ROOT_DIR/ChiaKey-Source"
HEADER_SHIMS="$SOURCE_DIR/Frameworks/HeaderShims"
EDITOR_DIR="$SOURCE_DIR/Utilities/PhraseEditor/OSX"
TESTS_DIR="$SOURCE_DIR/Utilities/PhraseEditor/Tests"
TMP_BASE="$(mktemp -d "${TMPDIR:-/tmp}/chiakey-legacy-import.XXXXXX")"

trap 'rm -rf "$TMP_BASE"' EXIT

clang++ \
  -std=c++17 \
  -ObjC++ \
  -fno-objc-arc \
  -I"$HEADER_SHIMS" \
  -I"$EDITOR_DIR" \
  -I"$SOURCE_DIR/Loaders/OSX-IMK" \
  -I"$SOURCE_DIR/Frameworks/Formosa/Headers" \
  -I"$SOURCE_DIR/Frameworks/Manjusri/Headers" \
  "$TESTS_DIR/TestLegacyImport.mm" \
  "$EDITOR_DIR/PEUserPhraseStore.mm" \
  "$SOURCE_DIR/Frameworks/Formosa/Source/Mandarin.cpp" \
  -framework Foundation \
  -framework AppKit \
  -lsqlite3 \
  -o "$TMP_BASE/TestLegacyImport"

# The store reads NSHomeDirectory(); CFFIXED_USER_HOME moves that to the
# temporary directory so the test cannot reach the real user profile. The test
# refuses to run if this fails to take effect.
mkdir -p "$TMP_BASE/home"
CFFIXED_USER_HOME="$TMP_BASE/home" "$TMP_BASE/TestLegacyImport" "$TMP_BASE/home"

echo "Legacy import tests passed."
