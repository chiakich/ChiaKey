#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="${ROOT_DIR}/ChiaKey-Source/Takao.xcodeproj"
DATA_TABLES_DIR="${ROOT_DIR}/ChiaKey-Source/DataTables"
DATABASES_DIR="${ROOT_DIR}/ChiaKey-Source/Distributions/Takao/CookedDatabase"
SMART_MANDARIN_DB="${DATABASES_DIR}/ChiaKeySource.db"
SMART_MANDARIN_DB_SCRIPT="${ROOT_DIR}/Scripts/build-dev-smart-mandarin-db.rb"
LEXICON_INSTALL_SCRIPT="${ROOT_DIR}/Scripts/install-lexicon-release.sh"
LOCAL_LEXICON_BUNDLE_SCRIPT="${ROOT_DIR}/Scripts/bundle-local-lexicon.sh"
ACTIVE_LEXICON_DB="${HOME}/Library/Application Support/ChiaKey/Lexicons/active/ChiaKeySource.db"
SCHEME="Takao-All"
APP_NAME="ChiaKey.app"
PROCESS_NAME="ChiaKey"
LEGACY_APP_NAME="千秋輸入法.app"
LEGACY_PROCESS_NAME="千秋輸入法"

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

usage() {
  cat <<EOF
Usage: Scripts/dev-install-local.sh [options]

Build and install the local IMK app into:
  ~/Library/Input Methods/${APP_NAME}

Options:
  --configuration Debug|Release  Build configuration. Default: ${CONFIGURATION}
  --derived-data-path PATH       DerivedData path. Default: ${DERIVED_DATA_PATH}
  --skip-build                   Reinstall the existing build product.
  --update-lexicon               Install the latest lexicon release after app install.
  --bundle-local-lexicon         Bundle the active local lexicon into the dev app.
  --local-lexicon PATH           Bundle this local ChiaKeySource.db into the dev app.
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
INSTALL_APP="${INSTALL_DIR}/${APP_NAME}"
LEGACY_INSTALL_APP="${INSTALL_DIR}/${LEGACY_APP_NAME}"

case "${INSTALL_APP}" in
  "${HOME}"/Library/Input\ Methods/*.app) ;;
  *)
    echo "Refusing to install outside ~/Library/Input Methods: ${INSTALL_APP}" >&2
    exit 1
    ;;
esac

if [[ "${SKIP_BUILD}" != "1" ]]; then
  run /usr/bin/xcodebuild \
    -project "${PROJECT}" \
    -scheme "${SCHEME}" \
    -configuration "${CONFIGURATION}" \
    -derivedDataPath "${DERIVED_DATA_PATH}" \
    CODE_SIGNING_ALLOWED=NO \
    ONLY_ACTIVE_ARCH=YES \
    build
fi

if [[ "${DRY_RUN}" != "1" && ! -d "${BUILT_APP}" ]]; then
  echo "Build product not found: ${BUILT_APP}" >&2
  exit 1
fi

run /bin/mkdir -p "${BUILT_RESOURCES}"
run /bin/rm -rf "${BUILT_RESOURCES}/DataTables"
run /usr/bin/ditto "${DATA_TABLES_DIR}" "${BUILT_RESOURCES}/DataTables"

if [[ ! -f "${SMART_MANDARIN_DB}" && -f "${SMART_MANDARIN_DB_SCRIPT}" ]]; then
  run /usr/bin/ruby "${SMART_MANDARIN_DB_SCRIPT}"
fi

if [[ "${DRY_RUN}" == "1" || -d "${DATABASES_DIR}" ]]; then
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

if [[ "${UPDATE_LEXICON}" == "1" && "${BUNDLE_LOCAL_LEXICON}" == "1" ]]; then
  run "${LEXICON_INSTALL_SCRIPT}" --skip-current
fi

if [[ "${BUNDLE_LOCAL_LEXICON}" == "1" ]]; then
  run_bundle_local_lexicon
fi

run /bin/mkdir -p "${INSTALL_DIR}"

if [[ "${DRY_RUN}" == "1" || -d "${INSTALL_APP}" ]]; then
  run /bin/rm -rf "${INSTALL_APP}"
fi

if [[ "${DRY_RUN}" == "1" || -d "${LEGACY_INSTALL_APP}" ]]; then
  run /bin/rm -rf "${LEGACY_INSTALL_APP}"
fi

run /usr/bin/ditto "${BUILT_APP}" "${INSTALL_APP}"
run /usr/bin/codesign --force --deep --sign - "${INSTALL_APP}"

if [[ "${UPDATE_LEXICON}" == "1" && "${BUNDLE_LOCAL_LEXICON}" != "1" ]]; then
  run "${LEXICON_INSTALL_SCRIPT}" --skip-current
fi

run_allow_fail /usr/bin/pkill -x "${PROCESS_NAME}"
run_allow_fail /usr/bin/pkill -f "${APP_NAME}/Contents/MacOS/${PROCESS_NAME}"
run_allow_fail /usr/bin/pkill -x "${LEGACY_PROCESS_NAME}"
run_allow_fail /usr/bin/pkill -f "${LEGACY_APP_NAME}/Contents/MacOS/${LEGACY_PROCESS_NAME}"

if [[ "${OPEN_SETTINGS}" == "1" ]]; then
  run /usr/bin/open "x-apple.systempreferences:com.apple.Keyboard-Settings.extension"
fi

cat <<EOF

Installed ${APP_NAME} to:
  ${INSTALL_APP}

For the first install, add it from System Settings > Keyboard > Text Input.
After later code changes, rerun this script and switch away from/back to the input method.
If macOS still caches an old copy, log out and back in once.
EOF
