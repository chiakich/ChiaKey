#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="$ROOT_DIR/ChiaKey-Source"
HEADER_SHIMS="$SOURCE_DIR/Frameworks/HeaderShims"

SDK_PATH="$(xcrun --sdk iphoneos --show-sdk-path)"

xcrun --sdk iphoneos clang \
  -x c \
  -fsyntax-only \
  -target arm64-apple-ios17.0 \
  -isysroot "$SDK_PATH" \
  -I"$SOURCE_DIR/Frameworks/ChiaKeyCore/Headers" \
  -include "$SOURCE_DIR/Frameworks/ChiaKeyCore/Headers/ChiaKeyCore/ChiaKeyCoreC.h" \
  /dev/null

xcrun --sdk iphoneos clang++ \
  -std=c++17 \
  -fsyntax-only \
  -target arm64-apple-ios17.0 \
  -isysroot "$SDK_PATH" \
  -DOV_USE_SQLITE \
  -I"$HEADER_SHIMS" \
  -I"$SOURCE_DIR/Frameworks/OpenVanilla/Headers" \
  -I"$SOURCE_DIR/Frameworks/PlainVanilla/Headers" \
  -I"$SOURCE_DIR/Frameworks/Formosa/Headers" \
  -I"$SOURCE_DIR/Frameworks/Manjusri/Headers" \
  -I"$SOURCE_DIR/Frameworks/ChiaKeyCore/Headers" \
  -I"$SOURCE_DIR/ModulePackages/OVIMMandarin" \
  "$SOURCE_DIR/Frameworks/ChiaKeyCore/Source/ChiaKeyCore.cpp" \
  "$SOURCE_DIR/Frameworks/ChiaKeyCore/Source/ChiaKeyCoreC.cpp" \
  "$SOURCE_DIR/Frameworks/Formosa/Source/Mandarin.cpp" \
  "$SOURCE_DIR/Frameworks/Manjusri/Source/Node.cpp" \
  "$SOURCE_DIR/ModulePackages/OVIMMandarin/OVIMSmartMandarin.cpp"
