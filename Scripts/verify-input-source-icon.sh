#!/bin/bash
#
# Fails when Info.plist's tsInputMethodIconFileKey names a file the built bundle
# does not contain. macOS resolves that icon inside the *client* app when the
# input source changes, and a dangling name has twice crashed apps on switch
# (ChiaKey16.icns, then ChiaKeyMenuIcon.png once COMBINE_HIDPI_IMAGES started
# merging the @2x pair into a .tiff).
#
# Usage: verify-input-source-icon.sh <path to .app>

set -euo pipefail

APP="${1:-}"
if [[ -z "${APP}" || ! -d "${APP}" ]]; then
  echo "verify-input-source-icon.sh: not an app bundle: ${APP}" >&2
  exit 1
fi

INFO_PLIST="${APP}/Contents/Info.plist"
if [[ ! -f "${INFO_PLIST}" ]]; then
  echo "verify-input-source-icon.sh: no Info.plist in ${APP}" >&2
  exit 1
fi

icon_file="$(/usr/libexec/PlistBuddy -c "Print :tsInputMethodIconFileKey" \
  "${INFO_PLIST}" 2>/dev/null || true)"
if [[ -z "${icon_file}" ]]; then
  echo "verify-input-source-icon.sh: tsInputMethodIconFileKey is not set in ${INFO_PLIST}" >&2
  exit 1
fi

if [[ ! -f "${APP}/Contents/Resources/${icon_file}" ]]; then
  echo "verify-input-source-icon.sh: tsInputMethodIconFileKey names '${icon_file}', which is not in ${APP}/Contents/Resources" >&2
  echo "Menu icons present:" >&2
  /bin/ls "${APP}/Contents/Resources" | /usr/bin/grep -i "menuicon" >&2 || true
  exit 1
fi

echo "verify-input-source-icon.sh: ${icon_file} is present"
