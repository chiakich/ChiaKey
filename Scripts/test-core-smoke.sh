#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="$ROOT_DIR/ChiaKey-Source"
HEADER_SHIMS="$SOURCE_DIR/Frameworks/HeaderShims"
ACTIVE_DB="${HOME}/Library/Application Support/ChiaKey/Lexicons/active/ChiaKeySource.db"
BUNDLED_DB="$SOURCE_DIR/Distributions/Takao/CookedDatabase/ChiaKeySource.db"
# A sibling of Lexicons, never inside it, so a download here cannot disturb the
# lexicon an installed ChiaKey is using.
CACHE_ROOT="${HOME}/Library/Application Support/ChiaKey/Lexicons-smoke-cache"
CACHED_DB="$CACHE_ROOT/active/ChiaKeySource.db"
TMP_BASE="$(mktemp -d "${TMPDIR:-/tmp}/chiakey-core-smoke.XXXXXX")"
TMP_WRITABLE="$TMP_BASE/writable"
SMOKE_BIN="$TMP_BASE/chiakey-core-smoke"

trap 'rm -rf "$TMP_BASE"' EXIT

mkdir -p "$TMP_WRITABLE"

LEXICON_DB="${CHIAKEY_LOCAL_LEXICON_DB:-}"
for candidate in "$ACTIVE_DB" "$BUNDLED_DB" "$CACHED_DB"; do
  [[ -n "$LEXICON_DB" ]] && break
  [[ -f "$candidate" ]] && LEXICON_DB="$candidate"
done

# Nothing local: fetch a release into the cache. install-lexicon-release.sh owns
# the verification rules, including the cross-origin SHA256SUMS check, so this
# must not grow its own download path.
if [[ -z "$LEXICON_DB" ]]; then
  if [[ "${CHIAKEY_SMOKE_NO_DOWNLOAD:-0}" == "1" ]]; then
    echo "No ChiaKeySource.db found and downloads are disabled." >&2
    echo "Looked at: $ACTIVE_DB, $BUNDLED_DB, $CACHED_DB" >&2
    exit 1
  fi
  echo "No ChiaKeySource.db found; downloading a release (about 50 MB) into:"
  echo "  $CACHE_ROOT"
  echo "Set CHIAKEY_SMOKE_NO_DOWNLOAD=1 to fail instead."
  "$ROOT_DIR/Scripts/install-lexicon-release.sh" --install-root "$CACHE_ROOT"
  LEXICON_DB="$CACHED_DB"
fi

if [[ ! -f "$LEXICON_DB" ]]; then
  echo "ChiaKeySource.db not found: $LEXICON_DB" >&2
  exit 1
fi
echo "Lexicon: $LEXICON_DB"

# Keep in sync with Scripts/test-ios-core-syntax.sh.
CORE_DEFINES=(
  -DOV_USE_SQLITE
  -DOVIMMANDARIN_IDENTIFIER='"Mandarin"'
  -DOVIMMANDARIN_PUNCTUATIONS_TABLE_PREFIX='"Punctuations"'
  -DOVIMSMARTMANDARIN_IDENTIFIER='"SmartMandarin"'
  -DOVIMTRADITIONALMANDARIN_IDENTIFIER='"TraditionalMandarin"'
  -DOVIMTRADITIONALMANDARIN_USE_ABSOLUTE_ORDER_QUERY_STRING=1
  -DOVAFASSOCIATEDPHRASE_IDENTIFIER='"AssociatedPhrase"'
)
CORE_SOURCES=(
  "$SOURCE_DIR/Frameworks/ChiaKeyCore/Source/ChiaKeyCore.cpp"
  "$SOURCE_DIR/Frameworks/ChiaKeyCore/Source/ChiaKeyCoreC.cpp"
  "$SOURCE_DIR/Frameworks/OpenVanilla/Source/OVFrameworkInfo.cpp"
  "$SOURCE_DIR/Frameworks/PlainVanilla/Source/PVPropertyListExpat.cpp"
  "$SOURCE_DIR/Frameworks/Formosa/Source/Mandarin.cpp"
  "$SOURCE_DIR/Frameworks/Manjusri/Source/Node.cpp"
  "$SOURCE_DIR/ModulePackages/OVIMMandarin/OVIMSmartMandarin.cpp"
  "$SOURCE_DIR/ModulePackages/OVIMMandarin/OVIMTraditionalMandarin.cpp"
  "$SOURCE_DIR/ModulePackages/OVIMMandarin/OVAFAssociatedPhrase.cpp"
)

clang++ \
  -std=c++17 \
  "${CORE_DEFINES[@]}" \
  -I"$HEADER_SHIMS" \
  -I"$SOURCE_DIR/Frameworks/OpenVanilla/Headers" \
  -I"$SOURCE_DIR/Frameworks/PlainVanilla/Headers" \
  -I"$SOURCE_DIR/Frameworks/Formosa/Headers" \
  -I"$SOURCE_DIR/Frameworks/Manjusri/Headers" \
  -I"$SOURCE_DIR/Frameworks/ChiaKeyCore/Headers" \
  -I"$SOURCE_DIR/ModulePackages/OVIMMandarin" \
  "$SOURCE_DIR/Frameworks/ChiaKeyCore/Tests/ChiaKeyCoreSmoke.cpp" \
  "${CORE_SOURCES[@]}" \
  -lsqlite3 -lexpat \
  -o "$SMOKE_BIN"

"$SMOKE_BIN" "$ROOT_DIR" "$TMP_WRITABLE" "$LEXICON_DB"
