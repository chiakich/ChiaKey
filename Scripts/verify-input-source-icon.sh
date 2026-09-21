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

# Check the selectable mode too: the parent icon alone is not sufficient for
# the input-source HUD. Keep this gate in both release and dev packaging.
mode_root=":ComponentInputModeDict"
mode_id="$(/usr/libexec/PlistBuddy -c "Print ${mode_root}:tsVisibleInputModeOrderedArrayKey:0" "${INFO_PLIST}")"
parent_id="$(/usr/libexec/PlistBuddy -c 'Print :TISInputSourceID' "${INFO_PLIST}")"
if [[ "${mode_id}" != "${parent_id}.Hant" ]]; then
  echo "verify-input-source-icon.sh: unexpected mode ID ${mode_id} for ${parent_id}" >&2
  exit 1
fi
mode_path="${mode_root}:tsInputModeListKey:${mode_id}"
mode_template="$(/usr/libexec/PlistBuddy -c "Print ${mode_path}:TISIconIsTemplate" "${INFO_PLIST}" 2>/dev/null || true)"
if [[ "${mode_template}" != "true" ]]; then
  echo "verify-input-source-icon.sh: ${mode_id} must declare TISIconIsTemplate=true for system tinting" >&2
  exit 1
fi
declared_id="$(/usr/libexec/PlistBuddy -c "Print ${mode_path}:TISInputSourceID" "${INFO_PLIST}")"
[[ "${declared_id}" == "${mode_id}" ]] || exit 1
for key in tsInputModeMenuIconFileKey tsInputModeAlternateMenuIconFileKey tsInputModePaletteIconFileKey; do
  mode_icon="$(/usr/libexec/PlistBuddy -c "Print ${mode_path}:${key}" "${INFO_PLIST}")"
  if [[ -z "${mode_icon}" || ! -f "${APP}/Contents/Resources/${mode_icon}" ]]; then
    echo "verify-input-source-icon.sh: ${key} names missing icon '${mode_icon}'" >&2
    exit 1
  fi
done
for localized_plist in "${APP}"/Contents/Resources/*.lproj/InfoPlist.strings; do
  [[ -f "${localized_plist}" ]] || continue
  mode_name="$(/usr/libexec/PlistBuddy -c "Print :${mode_id}" "${localized_plist}")"
  [[ -n "${mode_name}" ]] || exit 1
done
echo "verify-input-source-icon.sh: ${mode_id} icons and localized names are present"
