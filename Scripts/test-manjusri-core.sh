#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="$ROOT_DIR/ChiaKey-Source"
HEADER_SHIMS="$SOURCE_DIR/Frameworks/HeaderShims"
TESTS_DIR="$SOURCE_DIR/Frameworks/Manjusri/Tests"
TMP_BASE="$(mktemp -d "${TMPDIR:-/tmp}/chiakey-manjusri-core.XXXXXX")"

trap 'rm -rf "$TMP_BASE"' EXIT

# Graph/node lookups and Bopomofo syllable + keyboard layout round trips.
# Self-contained: no lexicon needed; the one graph test that needs a language
# model builds an in-memory SQLite fixture.
build_and_run() {
  local name="$1"
  shift
  local bin="$TMP_BASE/$name"

  clang++ \
    -std=c++17 \
    -I"$HEADER_SHIMS" \
    -I"$SOURCE_DIR/Frameworks/OpenVanilla/Headers" \
    -I"$SOURCE_DIR/Frameworks/PlainVanilla/Headers" \
    -I"$SOURCE_DIR/Frameworks/Formosa/Headers" \
    -I"$SOURCE_DIR/Frameworks/Manjusri/Headers" \
    "$TESTS_DIR/$name.cpp" \
    "$@" \
    -o "$bin"

  "$bin"
}

build_and_run TestNode "$SOURCE_DIR/Frameworks/Manjusri/Source/Node.cpp"
build_and_run TestGraphForcedBreak "$SOURCE_DIR/Frameworks/Manjusri/Source/Node.cpp" \
  -DOV_USE_SQLITE -lsqlite3
build_and_run TestMandarinSyllable "$SOURCE_DIR/Frameworks/Formosa/Source/Mandarin.cpp"

echo "Manjusri core tests passed."
