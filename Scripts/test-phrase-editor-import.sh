#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="$ROOT_DIR/ChiaKey-Source"
TESTS_DIR="$SOURCE_DIR/Utilities/PhraseEditor/Tests"
TMP_BASE="$(mktemp -d "${TMPDIR:-/tmp}/chiakey-phrase-editor-import.XXXXXX")"

trap 'rm -rf "$TMP_BASE"' EXIT

build() {
  local output="$1"
  shift
  clang++ \
    -std=c++17 \
    -ObjC++ \
    -fno-objc-arc \
    -DOV_USE_SQLITE \
    -I"$SOURCE_DIR/Frameworks/HeaderShims" \
    -I"$SOURCE_DIR/Frameworks/OpenVanilla/Headers" \
    -I"$SOURCE_DIR/Frameworks/PlainVanilla/Headers" \
    -I"$SOURCE_DIR/Frameworks/Formosa/Headers" \
    -I"$SOURCE_DIR/Frameworks/Manjusri/Headers" \
    -I"$SOURCE_DIR/Loaders/OSX-IMK" \
    -I"$SOURCE_DIR/Utilities/PhraseEditor/OSX" \
    "$@" \
    "$TESTS_DIR/TestPhraseEditorImport.mm" \
    "$SOURCE_DIR/Utilities/PhraseEditor/OSX/PEUserPhraseStore.mm" \
    "$SOURCE_DIR/Frameworks/Formosa/Source/Mandarin.cpp" \
    -framework Foundation \
    -framework AppKit \
    -lsqlite3 \
    -o "$output"
}

# CFFIXED_USER_HOME is what keeps the store off the real user profile; each
# test refuses to run if it did not take effect.
run_suite() {
  local binary="$1" home="$2"
  mkdir -p "$home"
  CFFIXED_USER_HOME="$home" "$binary" "$home"
}

# Shipping limits: the functional tests.
build "$TMP_BASE/TestPhraseEditorImport"
run_suite "$TMP_BASE/TestPhraseEditorImport" "$TMP_BASE/home-default"

# Lowered limits, so the size guards can be reached cheaply. The file limit
# stays at 1 MiB so a real export gets past it and the blob limit is what bites.
build "$TMP_BASE/TestPhraseEditorImportLimits" \
  -DPE_TEST_SMALL_LIMITS \
  -DPE_MAX_IMPORT_FILE_SIZE=1048576ULL \
  -DPE_MAX_LEARNING_BLOB_SIZE=64UL
run_suite "$TMP_BASE/TestPhraseEditorImportLimits" "$TMP_BASE/home-limits"

echo "Phrase editor import tests passed."
