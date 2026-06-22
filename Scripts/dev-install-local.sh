#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE="${ROOT_DIR}/KeyKey.xcworkspace"
SCHEME="Takao-All"
APP_NAME="Chiaki KeyKey.app"
PROCESS_NAME="Chiaki KeyKey"

CONFIGURATION="${CONFIGURATION:-Debug}"
DERIVED_DATA_PATH="${DERIVED_DATA_PATH:-${TMPDIR:-/tmp}/ChiakiKeyKeyDevInstall}"
INSTALL_DIR="${HOME}/Library/Input Methods"
SKIP_BUILD=0
DRY_RUN=0
OPEN_SETTINGS=0

usage() {
  cat <<EOF
Usage: Scripts/dev-install-local.sh [options]

Build and install the local IMK app into:
  ~/Library/Input Methods/${APP_NAME}

Options:
  --configuration Debug|Release  Build configuration. Default: ${CONFIGURATION}
  --derived-data-path PATH       DerivedData path. Default: ${DERIVED_DATA_PATH}
  --skip-build                   Reinstall the existing build product.
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

while [[ $# -gt 0 ]]; do
  case "$1" in
    --configuration)
      CONFIGURATION="${2:-}"
      shift 2
      ;;
    --derived-data-path)
      DERIVED_DATA_PATH="${2:-}"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=1
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
INSTALL_APP="${INSTALL_DIR}/${APP_NAME}"

case "${INSTALL_APP}" in
  "${HOME}"/Library/Input\ Methods/*.app) ;;
  *)
    echo "Refusing to install outside ~/Library/Input Methods: ${INSTALL_APP}" >&2
    exit 1
    ;;
esac

if [[ "${SKIP_BUILD}" != "1" ]]; then
  run /usr/bin/xcodebuild \
    -workspace "${WORKSPACE}" \
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

run /bin/mkdir -p "${INSTALL_DIR}"

if [[ "${DRY_RUN}" == "1" || -d "${INSTALL_APP}" ]]; then
  run /bin/rm -rf "${INSTALL_APP}"
fi

run /usr/bin/ditto "${BUILT_APP}" "${INSTALL_APP}"
run /usr/bin/codesign --force --deep --sign - "${INSTALL_APP}"

run_allow_fail /usr/bin/pkill -x "${PROCESS_NAME}"
run_allow_fail /usr/bin/pkill -f "${APP_NAME}/Contents/MacOS/${PROCESS_NAME}"

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
