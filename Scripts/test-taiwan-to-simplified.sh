#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HAN_DIR="$ROOT_DIR/ChiaKey-Source/ModulePackages/OVOFHanConvert"
GENERATOR="$ROOT_DIR/Scripts/generate-taiwan-to-simplified-table.py"
TMP_BASE="$(mktemp -d "${TMPDIR:-/tmp}/chiakey-tw2s.XXXXXX")"
trap 'rm -rf "$TMP_BASE"' EXIT

python3 "$GENERATOR" --check

build() {
  clang++ -std=c++17 -O1 -Wall -Wextra -Werror -I"$1" \
    "$HAN_DIR/Tests/TaiwanToSimplifiedTest.cpp" "$1/TaiwanToSimplified.cpp" -o "$2"
}

# Upstream cases run against OpenCC's data alone, since overrides change some.
UPSTREAM_DIR="$TMP_BASE/upstream"
mkdir -p "$UPSTREAM_DIR"
cp "$HAN_DIR/TaiwanToSimplified.h" "$HAN_DIR/TaiwanToSimplified.cpp" "$UPSTREAM_DIR/"
python3 "$GENERATOR" --overrides /dev/null --output "$UPSTREAM_DIR/TaiwanToSimplifiedTable.inc"
build "$UPSTREAM_DIR" "$TMP_BASE/upstream-test"
"$TMP_BASE/upstream-test" upstream "$HAN_DIR/OpenCC/tw2s-testcases.tsv"

build "$HAN_DIR" "$TMP_BASE/chiakey-test"
"$TMP_BASE/chiakey-test" chiakey

# The Mac output filters must hand the IME's words through intact.
SOURCE_DIR="$ROOT_DIR/ChiaKey-Source"
clang -c "$HAN_DIR/VXHCSC2TCTable.c" -o "$TMP_BASE/sc2tc.o"
clang++ -std=c++17 -O1 -Wno-deprecated-declarations \
  -I"$SOURCE_DIR/Frameworks/HeaderShims" \
  -I"$SOURCE_DIR/Frameworks/OpenVanilla/Headers" \
  -I"$HAN_DIR" -I"$SOURCE_DIR/ModulePackages/OVOFFullWidthCharacter" \
  "$HAN_DIR/Tests/OutputFilterSegmentsTest.cpp" \
  "$HAN_DIR/OVOFHanConvert.cpp" "$TMP_BASE/sc2tc.o" "$HAN_DIR/TaiwanToSimplified.cpp" \
  "$SOURCE_DIR/ModulePackages/OVOFFullWidthCharacter/OVOFFullWidthCharacter.cpp" \
  -o "$TMP_BASE/segments-test"
"$TMP_BASE/segments-test"
