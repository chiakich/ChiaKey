#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="$ROOT_DIR/ChiaKey-Source"
HEADER_SHIMS="$SOURCE_DIR/Frameworks/HeaderShims"
TESTS_DIR="$SOURCE_DIR/Frameworks/Manjusri/Tests"
TMP_BASE="$(mktemp -d "${TMPDIR:-/tmp}/chiakey-user-phrase-import.XXXXXX")"

trap 'rm -rf "$TMP_BASE"' EXIT

# The test includes BPMFUserPhraseHelper.cpp (it exercises the file-local
# export-block decryptor), so that source is not compiled separately here.
# The learning-block limit is lowered to 64 KiB so the test can cross it with a
# few hundred KiB of hex instead of the 192 MiB the shipping value would need.
# It stays well above the ~20 KiB block a real export carries, so every other
# test still runs against a block the importer accepts. The file-size limit is
# left as shipped: a sparse file reaches it for free.
clang++ \
  -std=c++17 \
  -DOV_USE_SQLITE \
  -DMJSR_MAX_LEARNING_BLOB_SIZE=65536UL \
  -I"$HEADER_SHIMS" \
  -I"$SOURCE_DIR/Frameworks/OpenVanilla/Headers" \
  -I"$SOURCE_DIR/Frameworks/PlainVanilla/Headers" \
  -I"$SOURCE_DIR/Frameworks/Formosa/Headers" \
  -I"$SOURCE_DIR/Frameworks/Minotaur/Headers" \
  -I"$SOURCE_DIR/Frameworks/Manjusri/Headers" \
  -I"$SOURCE_DIR/Frameworks/Manjusri/Source" \
  "$TESTS_DIR/TestUserPhraseImport.cpp" \
  "$SOURCE_DIR/Frameworks/Formosa/Source/Mandarin.cpp" \
  "$SOURCE_DIR/Frameworks/Minotaur/Source/Minotaur.cpp" \
  -lsqlite3 \
  -o "$TMP_BASE/TestUserPhraseImport"

"$TMP_BASE/TestUserPhraseImport" "$TMP_BASE"

echo "User phrase import tests passed."
