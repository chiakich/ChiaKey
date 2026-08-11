#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="${ROOT_DIR}/ChiaKey-Source/Takao.xcodeproj"
DATA_TABLES_DIR="${ROOT_DIR}/ChiaKey-Source/DataTables"
DATABASES_DIR="${ROOT_DIR}/ChiaKey-Source/Distributions/Takao/CookedDatabase"
SMART_MANDARIN_DB="${DATABASES_DIR}/ChiaKeySource.db"
LICENSE_FILE="${ROOT_DIR}/LICENSE"
COPYING_FILE="${ROOT_DIR}/ChiaKey-Source/COPYING"
ACKNOWLEDGEMENTS_FILE="${ROOT_DIR}/ChiaKey-Source/ACKNOWLEDGEMENTS"
EXTERNAL_LIBRARY_VERSIONS_FILE="${ROOT_DIR}/ChiaKey-Source/ExternalLibraries/VERSIONS.txt"
UNITTEST_COPYING_FILE="${ROOT_DIR}/ChiaKey-Source/ExternalLibraries/UnitTest++/COPYING"
EXPAT_COPYING_FILE="${ROOT_DIR}/ChiaKey-Source/ExternalLibraries/expat/COPYING"
ZLIB_README_FILE="${ROOT_DIR}/ChiaKey-Source/ExternalLibraries/zlib/README"
LEXICON_INSTALL_SCRIPT="${ROOT_DIR}/Scripts/install-lexicon-release.sh"
LOCAL_LEXICON_BUNDLE_SCRIPT="${ROOT_DIR}/Scripts/bundle-local-lexicon.sh"
ACTIVE_LEXICON_DB="${HOME}/Library/Application Support/ChiaKey/Lexicons/active/ChiaKeySource.db"
USER_SUPPORT_DIR="${HOME}/Library/Application Support/ChiaKey"
USER_PREFERENCES_DIR="${HOME}/Library/Preferences"
SCHEME="Takao-All"
# Name of the product Xcode builds; also the release bundle name.
APP_NAME="ChiaKey.app"
PROCESS_NAME="ChiaKey"
LEGACY_APP_NAME="千秋輸入法.app"
LEGACY_PROCESS_NAME="千秋輸入法"

# The dev install ships as a distinct input method
DEV_APP_NAME="ChiaKeyDev.app"
DEV_BUNDLE_ID="com.chiakey.inputmethod.ChiaKeyDev"
DEV_CONNECTION_NAME="ChiaKeyDev_1_Connection"
DEV_DISPLAY_NAME="千秋輸入法 (Dev)"

CONFIGURATION="${CONFIGURATION:-Debug}"
DEFAULT_TMP_DIR="${TMPDIR:-/tmp}"
DERIVED_DATA_PATH="${DERIVED_DATA_PATH:-${DEFAULT_TMP_DIR%/}/ChiaKeyDevInstall}"
INSTALL_DIR="${HOME}/Library/Input Methods"
SKIP_BUILD=0
DRY_RUN=0
OPEN_SETTINGS=0
UPDATE_LEXICON=0
BUNDLE_LOCAL_LEXICON=0
LOCAL_LEXICON_DB="${ACTIVE_LEXICON_DB}"
RESET_USER_STATE=0

usage() {
  cat <<EOF
Usage: Scripts/dev-install-local.sh [options]

Build and install the local IMK app as a distinct "dev" input method into:
  ~/Library/Input Methods/${DEV_APP_NAME}  (displayed as "${DEV_DISPLAY_NAME}")

It uses its own bundle id and connection name, so it coexists with a release
install without producing duplicate input source entries.

Options:
  --configuration Debug|Release  Build configuration. Default: ${CONFIGURATION}
  --derived-data-path PATH       DerivedData path. Default: ${DERIVED_DATA_PATH}
  --skip-build                   Reinstall the existing build product.
  --update-lexicon               Install the latest lexicon release after app install.
  --bundle-local-lexicon         Bundle the active local lexicon into the dev app.
  --local-lexicon PATH           Bundle this local ChiaKeySource.db into the dev app.
  --reset-user-state             Clear ChiaKey prefs, user DBs, and lexicon DBs before install.
  --dry-run                      Print commands without changing the system.
  --open-settings                Open Keyboard settings after install.
  -h, --help                     Show this help.
EOF
}

print_command() {
  printf '+'
  for arg in "$@"; do
    printf ' %q' "$arg"
  done
  printf '\n'
}

run() {
  print_command "$@"
  if [[ "${DRY_RUN}" != "1" ]]; then
    "$@"
  fi
}

run_allow_fail() {
  print_command "$@"
  if [[ "${DRY_RUN}" != "1" ]]; then
    "$@" >/dev/null 2>&1 || true
  fi
}

reset_user_state() {
  run_allow_fail /usr/bin/defaults delete com.chiakey.ChiaKey
  run_allow_fail /usr/bin/defaults delete com.chiakey.inputmethod.ChiaKey

  run /bin/rm -rf \
    "${USER_PREFERENCES_DIR}/com.chiakey.ChiaKey.plist" \
    "${USER_PREFERENCES_DIR}/com.chiakey.ChiaKey.Evaluator.plist" \
    "${USER_PREFERENCES_DIR}/com.chiakey.ChiaKey.Generic-cj-cin.plist" \
    "${USER_PREFERENCES_DIR}/com.chiakey.ChiaKey.SmartMandarin.plist" \
    "${USER_PREFERENCES_DIR}/com.chiakey.ChiaKey.TraditionalMandarin.plist" \
    "${USER_PREFERENCES_DIR}/com.chiakey.inputmethod.ChiaKey.plist"

  run /bin/rm -rf \
    "${USER_SUPPORT_DIR}/UserData.db" \
    "${USER_SUPPORT_DIR}/UserData.db-shm" \
    "${USER_SUPPORT_DIR}/UserData.db-wal" \
    "${USER_SUPPORT_DIR}/SmartMandarinUserData.db" \
    "${USER_SUPPORT_DIR}/SmartMandarinUserData.db-shm" \
    "${USER_SUPPORT_DIR}/SmartMandarinUserData.db-wal" \
    "${USER_SUPPORT_DIR}/Generic-cj-cin-DynamicCandidateOrder.sqlite3" \
    "${USER_SUPPORT_DIR}/Generic-cj-cin-DynamicCandidateOrder.sqlite3-shm" \
    "${USER_SUPPORT_DIR}/Generic-cj-cin-DynamicCandidateOrder.sqlite3-wal" \
    "${USER_SUPPORT_DIR}/DataTables" \
    "${USER_SUPPORT_DIR}/UserCannedMessages.txt" \
    "${USER_SUPPORT_DIR}/Lexicons"

  run_allow_fail /usr/bin/killall cfprefsd
}

plist_set_string() {
  local plist="$1"
  local key="$2"
  local value="$3"

  if [[ "${DRY_RUN}" == "1" ]]; then
    print_command /usr/libexec/PlistBuddy -c "Set :${key} ${value}" "${plist}"
    return
  fi

  print_command /usr/libexec/PlistBuddy -c "Set :${key} ${value}" "${plist}"
  if ! /usr/libexec/PlistBuddy -c "Set :${key} ${value}" "${plist}" >/dev/null 2>&1; then
    run /usr/libexec/PlistBuddy -c "Add :${key} string ${value}" "${plist}"
  fi
}

# The helper apps nested in Contents/SharedSupport ship with the release
# bundle identifiers. Left alone, a dev install and a release install are the
# same app as far as LaunchServices is concerned: opening the dev copy while
# the release one runs just activates the release window, so you end up
# staring at the build you did not just make. They also share one
# NSUserDefaults domain that way.
apply_dev_shared_support_identity() {
  local app
  local bundle_id
  local plist
  local localized_plist

  for app in Preferences PhraseEditor Updater; do
    plist="${INSTALL_APP}/Contents/SharedSupport/${app}.app/Contents/Info.plist"
    if [[ "${DRY_RUN}" != "1" && ! -f "${plist}" ]]; then
      continue
    fi

    bundle_id="$(dev_shared_support_bundle_id "${app}")"
    plist_set_string "${plist}" CFBundleIdentifier "${bundle_id}"
    plist_set_string "${plist}" CFBundleName "${app} (Dev)"
    plist_set_string "${plist}" CFBundleDisplayName "${app} (Dev)"

    for localized_plist in "${INSTALL_APP}/Contents/SharedSupport/${app}.app"/Contents/Resources/*.lproj/InfoPlist.strings; do
      [[ -f "${localized_plist}" ]] || continue
      plist_set_string "${localized_plist}" CFBundleName "${app} (Dev)"
      plist_set_string "${localized_plist}" CFBundleDisplayName "${app} (Dev)"
    done
  done
}

dev_shared_support_bundle_id() {
  printf '%s.%s\n' "${DEV_BUNDLE_ID}" "$1"
}

# Rewrite the installed bundle's identity so it registers as a distinct dev input method.
apply_dev_identity() {
  local localized_plist

  run /usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier ${DEV_BUNDLE_ID}" "${INSTALLED_PLIST}"
  run /usr/libexec/PlistBuddy -c "Set :TISInputSourceID ${DEV_BUNDLE_ID}" "${INSTALLED_PLIST}"
  run /usr/libexec/PlistBuddy -c "Set :InputMethodConnectionName ${DEV_CONNECTION_NAME}" "${INSTALLED_PLIST}"
  run /usr/libexec/PlistBuddy -c "Set :CFBundleName ${DEV_DISPLAY_NAME}" "${INSTALLED_PLIST}"
  run /usr/libexec/PlistBuddy -c "Add :CFBundleDisplayName string ${DEV_DISPLAY_NAME}" "${INSTALLED_PLIST}"

  # LSHasLocalizedDisplayName makes macOS prefer these localized values over
  # the bundle's Info.plist. Update them too, otherwise the input menu keeps
  # showing the release name even though the dev bundle identity is distinct.
  for localized_plist in "${INSTALL_APP}"/Contents/Resources/*.lproj/InfoPlist.strings; do
    [[ -f "${localized_plist}" ]] || continue
    run /usr/libexec/PlistBuddy -c "Set :CFBundleName ${DEV_DISPLAY_NAME}" "${localized_plist}"
    run /usr/libexec/PlistBuddy -c "Set :CFBundleDisplayName ${DEV_DISPLAY_NAME}" "${localized_plist}"
    run /usr/libexec/PlistBuddy -c "Set :com.chiakey.inputmethod.ChiaKey ${DEV_DISPLAY_NAME}" "${localized_plist}"
  done

  apply_dev_shared_support_identity
}

copy_legal_notices() {
  local legal_dir="${BUILT_RESOURCES}/Legal"
  local external_dir="${legal_dir}/ExternalLibraries"

  run /bin/rm -rf "${legal_dir}"
  run /bin/mkdir -p "${legal_dir}" "${external_dir}/UnitTest++" \
    "${external_dir}/expat" "${external_dir}/zlib"

  run /bin/cp "${LICENSE_FILE}" "${legal_dir}/LICENSE"
  run /bin/cp "${COPYING_FILE}" "${legal_dir}/ChiaKey-COPYING"
  run /bin/cp "${ACKNOWLEDGEMENTS_FILE}" "${legal_dir}/ACKNOWLEDGEMENTS"
  run /bin/cp "${EXTERNAL_LIBRARY_VERSIONS_FILE}" \
    "${external_dir}/VERSIONS.txt"
  run /bin/cp "${UNITTEST_COPYING_FILE}" "${external_dir}/UnitTest++/COPYING"
  run /bin/cp "${EXPAT_COPYING_FILE}" "${external_dir}/expat/COPYING"
  run /bin/cp "${ZLIB_README_FILE}" "${external_dir}/zlib/README"
}

run_bundle_local_lexicon() {
  local args=(
    "${LOCAL_LEXICON_BUNDLE_SCRIPT}"
    --app "${BUILT_APP}"
    --source "${LOCAL_LEXICON_DB}"
    --suppress-signing-note
  )

  if [[ "${DRY_RUN}" == "1" ]]; then
    args+=(--dry-run)
  fi

  print_command "${args[@]}"
  "${args[@]}"
}

require_lexicon_database_source() {
  if [[ "${DRY_RUN}" == "1" || -f "${SMART_MANDARIN_DB}" || "${BUNDLE_LOCAL_LEXICON}" == "1" ]]; then
    return
  fi

  cat >&2 <<EOF
Bundled fallback lexicon database not found:
  ${SMART_MANDARIN_DB}

This app repo no longer rebuilds ChiaKeySource.db from raw DataSource files.
Use a database produced by the lexicon repo or release pipeline, then either:
  - place it at the path above, or
  - rerun with --bundle-local-lexicon to use the active local lexicon, or
  - rerun with --local-lexicon /path/to/ChiaKeySource.db
EOF
  exit 1
}

require_value() {
  local option="$1"
  local value="${2:-}"

  if [[ -z "${value}" || "${value}" == --* ]]; then
    echo "${option} requires a value." >&2
    exit 2
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --configuration)
      require_value "$1" "${2:-}"
      CONFIGURATION="${2:-}"
      shift 2
      ;;
    --derived-data-path)
      require_value "$1" "${2:-}"
      DERIVED_DATA_PATH="${2:-}"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    --update-lexicon)
      UPDATE_LEXICON=1
      shift
      ;;
    --bundle-local-lexicon)
      BUNDLE_LOCAL_LEXICON=1
      shift
      ;;
    --local-lexicon)
      require_value "$1" "${2:-}"
      BUNDLE_LOCAL_LEXICON=1
      LOCAL_LEXICON_DB="$2"
      shift 2
      ;;
    --reset-user-state)
      RESET_USER_STATE=1
      shift
      ;;
    --dry-run)
      DRY_RUN=1
      shift
      ;;
    --open-settings)
      OPEN_SETTINGS=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

case "${CONFIGURATION}" in
  Debug|Release) ;;
  *)
    echo "Unsupported configuration: ${CONFIGURATION}" >&2
    exit 2
    ;;
esac

BUILT_APP="${DERIVED_DATA_PATH}/Build/Products/${CONFIGURATION}/${APP_NAME}"
BUILT_RESOURCES="${BUILT_APP}/Contents/Resources"
INSTALL_APP="${INSTALL_DIR}/${DEV_APP_NAME}"
INSTALLED_PLIST="${INSTALL_APP}/Contents/Info.plist"
LEGACY_INSTALL_APP="${INSTALL_DIR}/${LEGACY_APP_NAME}"
# Older revisions of this script installed the dev build under the release
# name; remove that stale copy so it does not linger as a second registration.
STALE_DEV_INSTALL_APP="${INSTALL_DIR}/${APP_NAME}"

case "${INSTALL_APP}" in
  "${HOME}"/Library/Input\ Methods/*.app) ;;
  *)
    echo "Refusing to install outside ~/Library/Input Methods: ${INSTALL_APP}" >&2
    exit 1
    ;;
esac

if [[ "${SKIP_BUILD}" != "1" ]]; then
  # Override the compiled-in IMK connection name so the dev build does not
  # fight the release build over the same Mach connection. $(inherited) keeps
  # each target's existing definitions; the appended macro wins (clang takes
  # the last -D for a given name) and only main.mm reads it, so this is inert
  # for every other target in the scheme.
  #
  # CHIAKEY_DEV_LOGGING rides along so the diagnostic os_log calls exist only in
  # dev installs. It keys off this script rather than Debug/Release because both
  # this and the release script can be pointed at either configuration.
  run /usr/bin/xcodebuild \
    -project "${PROJECT}" \
    -scheme "${SCHEME}" \
    -configuration "${CONFIGURATION}" \
    -derivedDataPath "${DERIVED_DATA_PATH}" \
    CODE_SIGNING_ALLOWED=NO \
    ONLY_ACTIVE_ARCH=YES \
    GCC_PREPROCESSOR_DEFINITIONS="\$(inherited) OPENVANILLA_CONNECTION_NAME=@\\\"${DEV_CONNECTION_NAME}\\\" CHIAKEY_DEV_LOGGING=1" \
    build
fi

if [[ "${DRY_RUN}" != "1" && ! -d "${BUILT_APP}" ]]; then
  echo "Build product not found: ${BUILT_APP}" >&2
  exit 1
fi

run /bin/mkdir -p "${BUILT_RESOURCES}"
run /bin/rm -rf "${BUILT_RESOURCES}/DataTables"
run /usr/bin/ditto "${DATA_TABLES_DIR}" "${BUILT_RESOURCES}/DataTables"

require_lexicon_database_source

if [[ "${DRY_RUN}" == "1" || -f "${SMART_MANDARIN_DB}" ]]; then
  run /bin/mkdir -p "${BUILT_RESOURCES}/Databases"
  run /usr/bin/ditto "${DATABASES_DIR}" "${BUILT_RESOURCES}/Databases"
  if [[ "${DRY_RUN}" == "1" || -f "${BUILT_RESOURCES}/Databases/ChiaKeySource.db" ]]; then
    run /bin/rm -f "${BUILT_RESOURCES}/Databases/KeyKeySource.db"
  fi
fi

if [[ "${DRY_RUN}" == "1" || -f "${LEXICON_INSTALL_SCRIPT}" ]]; then
  run /bin/mkdir -p "${BUILT_RESOURCES}/Scripts"
  run /bin/cp "${LEXICON_INSTALL_SCRIPT}" "${BUILT_RESOURCES}/Scripts/install-lexicon-release.sh"
fi

copy_legal_notices

if [[ "${UPDATE_LEXICON}" == "1" && "${BUNDLE_LOCAL_LEXICON}" == "1" ]]; then
  run "${LEXICON_INSTALL_SCRIPT}" --skip-current
fi

# Stop only the dev input method by its bundle path; a coexisting release
# install (same executable name) keeps running.
run_allow_fail /usr/bin/pkill -f "${DEV_APP_NAME}/Contents/MacOS/${PROCESS_NAME}"
run_allow_fail /usr/bin/pkill -f "${LEGACY_APP_NAME}/Contents/MacOS/${LEGACY_PROCESS_NAME}"

if [[ "${BUNDLE_LOCAL_LEXICON}" == "1" ]]; then
  run_bundle_local_lexicon
fi

if [[ "${RESET_USER_STATE}" == "1" ]]; then
  reset_user_state
fi

run /bin/mkdir -p "${INSTALL_DIR}"

if [[ "${DRY_RUN}" == "1" || -d "${INSTALL_APP}" ]]; then
  run /bin/rm -rf "${INSTALL_APP}"
fi

if [[ "${DRY_RUN}" == "1" || -d "${LEGACY_INSTALL_APP}" ]]; then
  run /bin/rm -rf "${LEGACY_INSTALL_APP}"
fi

# A ChiaKey.app in ~/Library may be an old dev copy (now superseded by the dev
# variant) or a per-user release install; we cannot tell them apart by path, so
# warn instead of deleting it — a stale dev copy here would still show as a
# duplicate entry.
if [[ "${DRY_RUN}" != "1" && -d "${STALE_DEV_INSTALL_APP}" ]]; then
  cat >&2 <<EOF
Note: ${STALE_DEV_INSTALL_APP} exists.
If it is an old dev build (this script now installs ${DEV_APP_NAME} instead),
remove it to avoid a duplicate entry:
  rm -rf "${STALE_DEV_INSTALL_APP}"
Leave it if it is your release install.
EOF
fi

run /usr/bin/ditto "${BUILT_APP}" "${INSTALL_APP}"
apply_dev_identity
# The `--deep` sign below does not descend into Contents/SharedSupport, so the
# nested helper apps would otherwise keep the linker's ad-hoc signature whose
# identifier is the bare executable name ("PhraseEditor"). Sign them first,
# each with the dev bundle id it was just given, so TCC (e.g. the Contacts
# prompt) attributes them correctly and does not merge them with the release
# copies; the outer `--deep` sign then seals them in. Signing inside-out
# matters: re-signing a nested app after the outer bundle would invalidate the
# outer signature's seal over SharedSupport.
for shared_support_app in Preferences PhraseEditor Updater; do
  shared_support_path="${INSTALL_APP}/Contents/SharedSupport/${shared_support_app}.app"
  if [[ "${DRY_RUN}" != "1" && ! -d "${shared_support_path}" ]]; then
    continue
  fi
  run /usr/bin/codesign --force --sign - \
    --identifier "$(dev_shared_support_bundle_id "${shared_support_app}")" \
    "${shared_support_path}"
done
run /usr/bin/codesign --force --deep --sign - "${INSTALL_APP}"

# Register the dev input source so it appears without a logout/login.
run_allow_fail "${INSTALL_APP}/Contents/MacOS/${PROCESS_NAME}" install

if [[ "${UPDATE_LEXICON}" == "1" && "${BUNDLE_LOCAL_LEXICON}" != "1" ]]; then
  run "${LEXICON_INSTALL_SCRIPT}" --skip-current
fi

# Stop only the dev input method by its bundle path; a coexisting release
# install (same executable name) keeps running.
run_allow_fail /usr/bin/pkill -f "${DEV_APP_NAME}/Contents/MacOS/${PROCESS_NAME}"
run_allow_fail /usr/bin/pkill -f "${LEGACY_APP_NAME}/Contents/MacOS/${LEGACY_PROCESS_NAME}"

if [[ "${OPEN_SETTINGS}" == "1" ]]; then
  run /usr/bin/open "x-apple.systempreferences:com.apple.Keyboard-Settings.extension"
fi

cat <<EOF

Installed the dev input method to:
  ${INSTALL_APP}
It registers separately as "${DEV_DISPLAY_NAME}" and coexists with a release install.

For the first install, add "${DEV_DISPLAY_NAME}" from System Settings > Keyboard > Text Input.
After later code changes, rerun this script and switch away from/back to the input method.
If macOS still caches an old copy, log out and back in once.
EOF
