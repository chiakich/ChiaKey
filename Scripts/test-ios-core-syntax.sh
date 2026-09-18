#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="$ROOT_DIR/ChiaKey-Source"
HEADER_SHIMS="$SOURCE_DIR/Frameworks/HeaderShims"

if [[ -z "${DEVELOPER_DIR:-}" && -d "/Applications/Xcode.app/Contents/Developer" ]]; then
  export DEVELOPER_DIR="/Applications/Xcode.app/Contents/Developer"
fi

SDK_PATH="$(xcrun --sdk iphoneos --show-sdk-path)"

xcrun --sdk iphoneos clang \
  -x c \
  -fsyntax-only \
  -target arm64-apple-ios17.0 \
  -isysroot "$SDK_PATH" \
  -I"$SOURCE_DIR/Frameworks/ChiaKeyCore/Headers" \
  -include "$SOURCE_DIR/Frameworks/ChiaKeyCore/Headers/ChiaKeyCore/ChiaKeyCoreC.h" \
  /dev/null

# Keep in sync with Scripts/test-core-smoke.sh.
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

xcrun --sdk iphoneos clang++ \
  -std=c++17 \
  -fsyntax-only \
  -target arm64-apple-ios17.0 \
  -isysroot "$SDK_PATH" \
  "${CORE_DEFINES[@]}" \
  -I"$HEADER_SHIMS" \
  -I"$SOURCE_DIR/Frameworks/OpenVanilla/Headers" \
  -I"$SOURCE_DIR/Frameworks/PlainVanilla/Headers" \
  -I"$SOURCE_DIR/Frameworks/Formosa/Headers" \
  -I"$SOURCE_DIR/Frameworks/Manjusri/Headers" \
  -I"$SOURCE_DIR/Frameworks/ChiaKeyCore/Headers" \
  -I"$SOURCE_DIR/ModulePackages/OVIMMandarin" \
  "${CORE_SOURCES[@]}"
