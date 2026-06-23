#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VALIDATOR="${ROOT_DIR}/Scripts/install-lexicon-release.sh"
APP_NAME="ChiaKey.app"
DEFAULT_TMP_DIR="${TMPDIR:-/tmp}"
DEFAULT_DERIVED_DATA_PATH="${DERIVED_DATA_PATH:-${DEFAULT_TMP_DIR%/}/ChiaKeyDevInstall}"
DEFAULT_APP="${DEFAULT_DERIVED_DATA_PATH%/}/Build/Products/${CONFIGURATION:-Debug}/${APP_NAME}"
DEFAULT_SOURCE="${CHIAKEY_LOCAL_LEXICON_DB:-${HOME}/Library/Application Support/ChiaKey/Lexicons/active/ChiaKeySource.db}"

APP_PATH="${DEFAULT_APP}"
SOURCE_DB="${DEFAULT_SOURCE}"
DRY_RUN=0
SKIP_VALIDATE=0
SUPPRESS_SIGNING_NOTE=0

usage() {
  cat <<EOF
Usage: Scripts/bundle-local-lexicon.sh [options]

Copy a local ChiaKeySource.db into a development ChiaKey.app bundle.

Options:
  --app PATH          Target ChiaKey.app. Default: ${DEFAULT_APP}
  --source PATH       Local ChiaKeySource.db. Default: ${DEFAULT_SOURCE}
  --skip-validate     Copy without running lexicon validation.
  --suppress-signing-note
                      Do not print the post-copy signing reminder.
  --dry-run           Print commands without changing files.
  -h, --help          Show this help.
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
    --app)
      require_value "$1" "${2:-}"
      APP_PATH="$2"
      shift 2
      ;;
    --source|--db)
      require_value "$1" "${2:-}"
      SOURCE_DB="$2"
      shift 2
      ;;
    --skip-validate)
      SKIP_VALIDATE=1
      shift
      ;;
    --suppress-signing-note)
      SUPPRESS_SIGNING_NOTE=1
      shift
      ;;
    --dry-run)
      DRY_RUN=1
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

TARGET_RESOURCES="${APP_PATH}/Contents/Resources"
TARGET_DATABASES="${TARGET_RESOURCES}/Databases"
TARGET_DB="${TARGET_DATABASES}/ChiaKeySource.db"
LEGACY_TARGET_DB="${TARGET_DATABASES}/KeyKeySource.db"

if [[ "${DRY_RUN}" != "1" ]]; then
  if [[ ! -f "${SOURCE_DB}" ]]; then
    echo "Local lexicon database not found: ${SOURCE_DB}" >&2
    exit 1
  fi

  if [[ ! -d "${APP_PATH}" ]]; then
    echo "Target app bundle not found: ${APP_PATH}" >&2
    exit 1
  fi

  if [[ ! -d "${TARGET_RESOURCES}" ]]; then
    echo "Target app bundle has no Contents/Resources directory: ${APP_PATH}" >&2
    exit 1
  fi

  if [[ "${SKIP_VALIDATE}" != "1" ]]; then
    "${VALIDATOR}" --validate-db "${SOURCE_DB}"
  fi
fi

run /bin/mkdir -p "${TARGET_DATABASES}"
run /bin/cp "${SOURCE_DB}" "${TARGET_DB}"
run /bin/rm -f "${LEGACY_TARGET_DB}"

cat <<EOF

Bundled local lexicon:
  source: ${SOURCE_DB}
  target: ${TARGET_DB}
EOF

if [[ "${SUPPRESS_SIGNING_NOTE}" != "1" ]]; then
  cat <<EOF
If this app was already signed, sign it again before launching.
EOF
fi
