#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="$ROOT_DIR/ChiaKey-Source"
HEADER_SHIMS="$SOURCE_DIR/Frameworks/HeaderShims"
ACTIVE_DB="${HOME}/Library/Application Support/ChiaKey/Lexicons/active/ChiaKeySource.db"
BUNDLED_DB="$SOURCE_DIR/Distributions/Takao/CookedDatabase/ChiaKeySource.db"
TMP_BASE="$(mktemp -d "${TMPDIR:-/tmp}/chiakey-core-smoke.XXXXXX")"
TMP_WRITABLE="$TMP_BASE/writable"
SMOKE_BIN="$TMP_BASE/chiakey-core-smoke"

trap 'rm -rf "$TMP_BASE"' EXIT

mkdir -p "$TMP_WRITABLE"

LEXICON_DB="${CHIAKEY_LOCAL_LEXICON_DB:-}"
if [[ -z "$LEXICON_DB" && -f "$ACTIVE_DB" ]]; then
  LEXICON_DB="$ACTIVE_DB"
fi
if [[ -z "$LEXICON_DB" ]]; then
  LEXICON_DB="$BUNDLED_DB"
fi
if [[ ! -f "$LEXICON_DB" ]]; then
  echo "ChiaKeySource.db not found: $LEXICON_DB" >&2
  exit 1
fi

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

"$SMOKE_BIN" "$ROOT_DIR" "$TMP_WRITABLE" "$LEXICON_DB"
