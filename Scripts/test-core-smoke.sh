#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="$ROOT_DIR/ChiaKey-Source"
HEADER_SHIMS="$SOURCE_DIR/Frameworks/HeaderShims"
TMP_BASE="$(mktemp -d "${TMPDIR:-/tmp}/chiakey-core-smoke.XXXXXX")"
TMP_WRITABLE="$TMP_BASE/writable"
SMOKE_BIN="$TMP_BASE/chiakey-core-smoke"

trap 'rm -rf "$TMP_BASE"' EXIT

mkdir -p "$TMP_WRITABLE"

clang++ \
  -std=c++17 \
  -DOV_USE_SQLITE \
  -I"$HEADER_SHIMS" \
  -I"$SOURCE_DIR/Frameworks/OpenVanilla/Headers" \
  -I"$SOURCE_DIR/Frameworks/PlainVanilla/Headers" \
  -I"$SOURCE_DIR/Frameworks/Formosa/Headers" \
  -I"$SOURCE_DIR/Frameworks/Manjusri/Headers" \
  -I"$SOURCE_DIR/Frameworks/ChiaKeyCore/Headers" \
  -I"$SOURCE_DIR/ModulePackages/OVIMMandarin" \
  "$SOURCE_DIR/Frameworks/ChiaKeyCore/Tests/ChiaKeyCoreSmoke.cpp" \
  "$SOURCE_DIR/Frameworks/ChiaKeyCore/Source/ChiaKeyCore.cpp" \
  "$SOURCE_DIR/Frameworks/ChiaKeyCore/Source/ChiaKeyCoreC.cpp" \
  "$SOURCE_DIR/Frameworks/Formosa/Source/Mandarin.cpp" \
  "$SOURCE_DIR/Frameworks/Manjusri/Source/Node.cpp" \
  "$SOURCE_DIR/ModulePackages/OVIMMandarin/OVIMSmartMandarin.cpp" \
  -lsqlite3 \
  -o "$SMOKE_BIN"

"$SMOKE_BIN" "$ROOT_DIR" "$TMP_WRITABLE"
