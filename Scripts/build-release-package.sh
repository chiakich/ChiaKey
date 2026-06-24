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
CONFIGURATION="${CONFIGURATION:-Release}"
DEFAULT_TMP_DIR="${TMPDIR:-/tmp}"
WORK_DIR="${WORK_DIR:-${DEFAULT_TMP_DIR%/}/ChiaKeyReleasePackage}"
DERIVED_DATA_PATH="${DERIVED_DATA_PATH:-${WORK_DIR}/DerivedData}"
OUTPUT_DIR="${OUTPUT_DIR:-${ROOT_DIR}/artifacts/release}"
APP_SIGN_IDENTITY="${APP_SIGN_IDENTITY:--}"
INSTALLER_SIGN_IDENTITY="${INSTALLER_SIGN_IDENTITY:-}"
NOTARY_PROFILE="${NOTARY_PROFILE:-}"
PACKAGE_IDENTIFIER="${PACKAGE_IDENTIFIER:-com.chiakey.inputmethod.ChiaKey.pkg}"
COMPONENT_IDENTIFIER="${COMPONENT_IDENTIFIER:-com.chiakey.inputmethod.ChiaKey.component}"
MIN_OS_VERSION="${MIN_OS_VERSION:-11.0}"

# Keep macOS resource forks out of the installer payload as AppleDouble files.
export COPYFILE_DISABLE=1

SKIP_BUILD=0
BUNDLE_LOCAL_LEXICON=0
LOCAL_LEXICON_DB="${ACTIVE_LEXICON_DB}"
NOTARIZE=0
STAPLE=1
KEEP_WORK_DIR=0
PKG_NAME=""

usage() {
  cat <<EOF
Usage: Scripts/build-release-package.sh [options]

Build a release installer package for ChiaKey.

The generated package installs:
  /Library/Input Methods/${APP_NAME}

Options:
  --configuration Debug|Release      Build configuration. Default: ${CONFIGURATION}
  --derived-data-path PATH           DerivedData path. Default: ${DERIVED_DATA_PATH}
  --work-dir PATH                    Temporary packaging workspace. Default: ${WORK_DIR}
  --output-dir PATH                  Package output directory. Default: ${OUTPUT_DIR}
  --pkg-name NAME                    Output package filename.
  --skip-build                       Package the existing build product.
  --app-sign-identity ID             App signing identity. Default: ${APP_SIGN_IDENTITY}
  --installer-sign-identity ID       Installer signing identity. Default: INSTALLER_SIGN_IDENTITY env.
  --notarize                         Submit the package with notarytool.
  --notary-profile PROFILE           notarytool keychain profile. Default: NOTARY_PROFILE env.
  --skip-staple                      Do not staple after notarization.
  --bundle-local-lexicon             Bundle the active local lexicon into the app.
  --local-lexicon PATH               Bundle this local ChiaKeySource.db into the app.
  --keep-work-dir                    Keep temporary staging files.
  -h, --help                         Show this help.

Signing examples:
  APP_SIGN_IDENTITY="Developer ID Application: Example" \\
  INSTALLER_SIGN_IDENTITY="Developer ID Installer: Example" \\
  NOTARY_PROFILE="chiakey-notary" \\
    Scripts/build-release-package.sh --notarize

Without signing identities, the app is ad-hoc signed and the package is
unsigned. That is useful for local installer testing, not public release.
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
  "$@"
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
      CONFIGURATION="$2"
      shift 2
      ;;
    --derived-data-path)
      require_value "$1" "${2:-}"
      DERIVED_DATA_PATH="$2"
      shift 2
      ;;
    --work-dir)
      require_value "$1" "${2:-}"
      WORK_DIR="$2"
      shift 2
      ;;
    --output-dir)
      require_value "$1" "${2:-}"
      OUTPUT_DIR="$2"
      shift 2
      ;;
    --pkg-name)
      require_value "$1" "${2:-}"
      PKG_NAME="$2"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    --app-sign-identity)
      require_value "$1" "${2:-}"
      APP_SIGN_IDENTITY="$2"
      shift 2
      ;;
    --installer-sign-identity)
      require_value "$1" "${2:-}"
      INSTALLER_SIGN_IDENTITY="$2"
      shift 2
      ;;
    --notarize)
      NOTARIZE=1
      shift
      ;;
    --notary-profile)
      require_value "$1" "${2:-}"
      NOTARY_PROFILE="$2"
      shift 2
      ;;
    --skip-staple)
      STAPLE=0
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
    --keep-work-dir)
      KEEP_WORK_DIR=1
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

if [[ "${NOTARIZE}" == "1" ]]; then
  if [[ -z "${NOTARY_PROFILE}" ]]; then
    echo "--notarize requires --notary-profile or NOTARY_PROFILE." >&2
    exit 2
  fi

  if [[ "${APP_SIGN_IDENTITY}" == "-" ]]; then
    echo "--notarize requires a Developer ID Application app signing identity." >&2
    exit 2
  fi

  if [[ -z "${INSTALLER_SIGN_IDENTITY}" ]]; then
    echo "--notarize requires a Developer ID Installer signing identity." >&2
    exit 2
  fi
fi

BUILT_APP="${DERIVED_DATA_PATH}/Build/Products/${CONFIGURATION}/${APP_NAME}"
BUILT_RESOURCES="${BUILT_APP}/Contents/Resources"
STAGE_ROOT="${WORK_DIR}/pkg-root"
PKG_SCRIPTS_DIR="${WORK_DIR}/pkg-scripts"
COMPONENT_PKG="${WORK_DIR}/ChiaKey-component.pkg"
EXPANDED_COMPONENT_DIR="${WORK_DIR}/ChiaKey-component-expanded"
CLEAN_COMPONENT_PKG="${WORK_DIR}/ChiaKey-component-clean.pkg"

rebuild_component_package_without_metadata_payload() {
  # Recent macOS pkgbuild serializes provenance xattrs as AppleDouble payload
  # entries. Keep pkgbuild's metadata, but replace Payload with plain cpio.
  run /bin/rm -rf "${EXPANDED_COMPONENT_DIR}" "${CLEAN_COMPONENT_PKG}"
  run /usr/sbin/pkgutil --expand "${COMPONENT_PKG}" "${EXPANDED_COMPONENT_DIR}"

  printf '+ rebuild Payload with cpio -R 0:0\n'
  (
    cd "${STAGE_ROOT}"
    /usr/bin/find . -print \
      | /usr/bin/cpio -o --format odc -R 0:0 \
      | /usr/bin/gzip -c > "${EXPANDED_COMPONENT_DIR}/Payload"
  )

  run /usr/bin/mkbom "${STAGE_ROOT}" "${EXPANDED_COMPONENT_DIR}/Bom"

  run /usr/bin/find "${EXPANDED_COMPONENT_DIR}" -name "._*" -delete
  run /usr/bin/find "${EXPANDED_COMPONENT_DIR}" -name ".DS_Store" -delete
  run /usr/bin/xattr -cr "${EXPANDED_COMPONENT_DIR}"
  run /usr/sbin/pkgutil --flatten "${EXPANDED_COMPONENT_DIR}" "${CLEAN_COMPONENT_PKG}"
}

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

if [[ ! -d "${BUILT_APP}" ]]; then
  echo "Build product not found: ${BUILT_APP}" >&2
  exit 1
fi

run /bin/mkdir -p "${BUILT_RESOURCES}"
run /bin/rm -rf "${BUILT_RESOURCES}/DataTables"
run /usr/bin/ditto --norsrc "${DATA_TABLES_DIR}" "${BUILT_RESOURCES}/DataTables"

if [[ ! -f "${SMART_MANDARIN_DB}" && -f "${SMART_MANDARIN_DB_SCRIPT}" ]]; then
  run /usr/bin/ruby "${SMART_MANDARIN_DB_SCRIPT}"
fi

if [[ -d "${DATABASES_DIR}" ]]; then
  run /bin/mkdir -p "${BUILT_RESOURCES}/Databases"
  run /usr/bin/ditto --norsrc "${DATABASES_DIR}" "${BUILT_RESOURCES}/Databases"
  if [[ -f "${BUILT_RESOURCES}/Databases/ChiaKeySource.db" ]]; then
    run /bin/rm -f "${BUILT_RESOURCES}/Databases/KeyKeySource.db"
  fi
fi

if [[ -f "${LEXICON_INSTALL_SCRIPT}" ]]; then
  run /bin/mkdir -p "${BUILT_RESOURCES}/Scripts"
  run /bin/cp "${LEXICON_INSTALL_SCRIPT}" "${BUILT_RESOURCES}/Scripts/install-lexicon-release.sh"
fi

if [[ "${BUNDLE_LOCAL_LEXICON}" == "1" ]]; then
  run "${LOCAL_LEXICON_BUNDLE_SCRIPT}" \
    --app "${BUILT_APP}" \
    --source "${LOCAL_LEXICON_DB}" \
    --suppress-signing-note
fi

run /usr/bin/find "${BUILT_RESOURCES}" -name ".gitignore" -delete
run /usr/bin/find "${BUILT_APP}" -name "._*" -delete
run /usr/bin/find "${BUILT_APP}" -name ".DS_Store" -delete
run /usr/bin/xattr -cr "${BUILT_APP}"

if [[ "${APP_SIGN_IDENTITY}" == "-" ]]; then
  run /usr/bin/codesign --force --deep --sign - "${BUILT_APP}"
else
  run /usr/bin/codesign --force --deep --options runtime --timestamp \
    --sign "${APP_SIGN_IDENTITY}" "${BUILT_APP}"
fi

run /usr/bin/xattr -cr "${BUILT_APP}"
run /usr/bin/codesign --verify --deep --strict "${BUILT_APP}"

VERSION="$(/usr/libexec/PlistBuddy -c "Print :CFBundleVersion" "${BUILT_APP}/Contents/Info.plist")"
if [[ -z "${PKG_NAME}" ]]; then
  PKG_NAME="ChiaKey-${VERSION}.pkg"
fi

OUTPUT_PKG="${OUTPUT_DIR}/${PKG_NAME}"

run /bin/rm -rf "${STAGE_ROOT}" "${PKG_SCRIPTS_DIR}" "${COMPONENT_PKG}"
run /bin/mkdir -p "${STAGE_ROOT}/Library/Input Methods" "${PKG_SCRIPTS_DIR}" "${OUTPUT_DIR}"
run /usr/bin/ditto --norsrc "${BUILT_APP}" "${STAGE_ROOT}/Library/Input Methods/${APP_NAME}"
run /usr/bin/find "${STAGE_ROOT}" -name "._*" -delete
run /usr/bin/find "${STAGE_ROOT}" -name ".DS_Store" -delete
run /usr/bin/xattr -cr "${STAGE_ROOT}"

cat >"${PKG_SCRIPTS_DIR}/postinstall" <<'POSTINSTALL'
#!/bin/sh

/usr/bin/pkill -x ChiaKey >/dev/null 2>&1 || true
/usr/bin/pkill -f '/Library/Input Methods/ChiaKey.app/Contents/MacOS/ChiaKey' >/dev/null 2>&1 || true

exit 0
POSTINSTALL
run /bin/chmod 755 "${PKG_SCRIPTS_DIR}/postinstall"

run /usr/bin/pkgbuild \
  --root "${STAGE_ROOT}" \
  --identifier "${COMPONENT_IDENTIFIER}" \
  --version "${VERSION}" \
  --install-location "/" \
  --scripts "${PKG_SCRIPTS_DIR}" \
  --ownership recommended \
  --min-os-version "${MIN_OS_VERSION}" \
  "${COMPONENT_PKG}"

rebuild_component_package_without_metadata_payload

PRODUCTBUILD_ARGS=(
  /usr/bin/productbuild
  --package "${CLEAN_COMPONENT_PKG}"
  --identifier "${PACKAGE_IDENTIFIER}"
  --version "${VERSION}"
)

if [[ -n "${INSTALLER_SIGN_IDENTITY}" ]]; then
  PRODUCTBUILD_ARGS+=(--sign "${INSTALLER_SIGN_IDENTITY}")
fi

PRODUCTBUILD_ARGS+=("${OUTPUT_PKG}")
run "${PRODUCTBUILD_ARGS[@]}"

if [[ "${NOTARIZE}" == "1" ]]; then
  run /usr/bin/xcrun notarytool submit "${OUTPUT_PKG}" \
    --keychain-profile "${NOTARY_PROFILE}" \
    --wait

  if [[ "${STAPLE}" == "1" ]]; then
    run /usr/bin/xcrun stapler staple "${OUTPUT_PKG}"
  fi
fi

if [[ "${KEEP_WORK_DIR}" != "1" ]]; then
  run /bin/rm -rf \
    "${STAGE_ROOT}" \
    "${PKG_SCRIPTS_DIR}" \
    "${COMPONENT_PKG}" \
    "${EXPANDED_COMPONENT_DIR}" \
    "${CLEAN_COMPONENT_PKG}"
fi

cat <<EOF

Built installer package:
  ${OUTPUT_PKG}

Install target:
  /Library/Input Methods/${APP_NAME}

EOF

if [[ -z "${INSTALLER_SIGN_IDENTITY}" ]]; then
  cat <<EOF
Note: this package is unsigned. Use --installer-sign-identity for public release.
EOF
fi

if [[ "${APP_SIGN_IDENTITY}" == "-" ]]; then
  cat <<EOF
Note: the app is ad-hoc signed. Use --app-sign-identity for public release.
EOF
fi
