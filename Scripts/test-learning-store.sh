#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="$ROOT_DIR/ChiaKey-Source"
HEADER_SHIMS="$SOURCE_DIR/Frameworks/HeaderShims"
TESTS_DIR="$SOURCE_DIR/Frameworks/Manjusri/Tests"
TMP_BASE="$(mktemp -d "${TMPDIR:-/tmp}/chiakey-learning-store.XXXXXX")"

trap 'rm -rf "$TMP_BASE"' EXIT

# Self-contained: these tests build their own SQLite fixtures, so no lexicon.
build_and_run() {
  local name="$1"
  local bin="$TMP_BASE/$name"

  clang++ \
    -std=c++17 \
    -DOV_USE_SQLITE \
    -I"$HEADER_SHIMS" \
    -I"$SOURCE_DIR/Frameworks/OpenVanilla/Headers" \
    -I"$SOURCE_DIR/Frameworks/PlainVanilla/Headers" \
    -I"$SOURCE_DIR/Frameworks/Formosa/Headers" \
    -I"$SOURCE_DIR/Frameworks/Manjusri/Headers" \
    -I"$SOURCE_DIR/ModulePackages/OVIMMandarin" \
    "$TESTS_DIR/$name.cpp" \
    "$SOURCE_DIR/Frameworks/Manjusri/Source/Node.cpp" \
    "$SOURCE_DIR/Frameworks/Formosa/Source/Mandarin.cpp" \
    -lsqlite3 \
    -o "$bin"

  "$bin" "$TMP_BASE"
}

build_and_run TestLearningStore
build_and_run TestLearnedPickSurvivesWalk
# Drives ManjusriComposer, hence the OVIMMandarin include and Mandarin.cpp above.
build_and_run TestLearningReversal

echo "Learning store tests passed."
