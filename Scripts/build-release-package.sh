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
INSTALLER_TEMPLATE_DIR="${ROOT_DIR}/Packaging/Installer"
INSTALLER_DISTRIBUTION_TEMPLATE="${INSTALLER_TEMPLATE_DIR}/Distribution.xml.in"
INSTALLER_RESOURCES_DIR="${INSTALLER_TEMPLATE_DIR}/Resources"
INSTALLER_SCRIPTS_DIR="${INSTALLER_TEMPLATE_DIR}/Scripts"
ACTIVE_LEXICON_DB="${HOME}/Library/Application Support/ChiaKey/Lexicons/active/ChiaKeySource.db"
LEXICON_RELEASE_REPO="${LEXICON_RELEASE_REPO:-akira02/ChiaKey-Lexicon}"
LEXICON_RELEASE_TAG="${LEXICON_RELEASE_TAG:-}"
LEXICON_RELEASE_MANIFEST_URL="${LEXICON_RELEASE_MANIFEST_URL:-}"

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
VERBOSE="${VERBOSE:-0}"
BUNDLE_LOCAL_LEXICON=0
LOCAL_LEXICON_DB="${ACTIVE_LEXICON_DB}"
BUNDLE_RELEASE_LEXICON=1
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
  --verbose                          Show full xcodebuild output. Default: quiet.
  --app-sign-identity ID             App signing identity. Default: ${APP_SIGN_IDENTITY}
  --installer-sign-identity ID       Installer signing identity. Default: INSTALLER_SIGN_IDENTITY env.
  --notarize                         Submit the package with notarytool.
  --notary-profile PROFILE           notarytool keychain profile. Default: NOTARY_PROFILE env.
  --skip-staple                      Do not staple after notarization.
  --bundle-local-lexicon             Bundle the active local lexicon into the app.
                                     Overrides the default GitHub release lexicon.
  --local-lexicon PATH               Bundle this local ChiaKeySource.db into the app.
                                     Overrides the default GitHub release lexicon.
  --bundle-release-lexicon           Download and bundle a ChiaKey-Lexicon release.
                                     This is the default release behavior.
  --lexicon-repo OWNER/REPO          Lexicon repo. Default: ${LEXICON_RELEASE_REPO}
  --lexicon-tag TAG                  Lexicon release tag. Default: latest release.
  --lexicon-manifest-url URL         Manifest URL. Overrides --lexicon-repo/tag.
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
  if [[ "${VERBOSE}" == "1" ]]; then
    print_command "$@"
  fi
  "$@"
}

# Drop known-benign xcodebuild noise (quiet mode only). awk always exits 0 so
# a real xcodebuild failure still propagates via pipefail.
filter_build_noise() {
  /usr/bin/awk '
    /has no symbols/ { next }
    /will be run during every build because/ { next }
    { print }
  '
}

download() {
  local output="$1"
  local url="$2"

  run /usr/bin/curl -fL --retry 3 --silent --show-error \
    --header "User-Agent: ChiaKey Release Packager" \
    --output "${output}" "${url}"
}

verify_sha256() {
  local file="$1"
  local expected="$2"
  local actual

  actual="$(/usr/bin/shasum -a 256 "${file}" | /usr/bin/awk '{print $1}')"
  if [[ "${actual}" != "${expected}" ]]; then
    echo "SHA-256 mismatch for ${file}" >&2
    echo "  expected: ${expected}" >&2
    echo "  actual:   ${actual}" >&2
    exit 1
  fi
}

validate_manifest_path_component() {
  local label="$1"
  local value="$2"

  if [[ -z "${value}" || "${value}" == "." || "${value}" == ".." ||
        ! "${value}" =~ ^[0-9A-Za-z._-]+$ ]]; then
    echo "Unsafe ${label} in lexicon manifest: ${value}" >&2
    exit 1
  fi
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

copy_release_lexicon() {
  local lexicon_dir="${WORK_DIR}/release-lexicon"
  local manifest_file="${lexicon_dir}/lexicon-manifest.json"
  local manifest_url="${LEXICON_RELEASE_MANIFEST_URL}"

  run /bin/rm -rf "${lexicon_dir}"
  run /bin/mkdir -p "${lexicon_dir}"

  if [[ -z "${manifest_url}" ]]; then
    if [[ -n "${LEXICON_RELEASE_TAG}" ]]; then
      manifest_url="https://github.com/${LEXICON_RELEASE_REPO}/releases/download/${LEXICON_RELEASE_TAG}/lexicon-manifest.json"
    else
      manifest_url="https://github.com/${LEXICON_RELEASE_REPO}/releases/latest/download/lexicon-manifest.json"
    fi
  fi

  echo "Downloading lexicon release manifest:"
  echo "  ${manifest_url}"
  download "${manifest_file}" "${manifest_url}"

  local artifact_info
  artifact_info="$(
    /usr/bin/ruby -rjson - "${manifest_file}" <<'RUBY'
manifest_path = ARGV.fetch(0)
manifest = JSON.parse(File.read(manifest_path))

db = manifest.fetch("artifacts").find { |artifact| artifact["kind"] == "chiakey-source-db" }
metadata = manifest.fetch("artifacts").find { |artifact| artifact["kind"] == "metadata" }
abort "manifest does not contain a chiakey-source-db artifact" unless db

fields = [
  manifest.fetch("version"),
  manifest.fetch("database_schema_version"),
  db.fetch("url"),
  db.fetch("filename"),
  db.fetch("sha256"),
  metadata&.fetch("url", ""),
  metadata&.fetch("filename", ""),
  metadata&.fetch("sha256", "")
]
puts fields.join("\t")
RUBY
  )"

  local version db_schema_version db_url db_filename db_sha metadata_url metadata_filename metadata_sha
  IFS=$'\t' read -r version db_schema_version db_url db_filename db_sha metadata_url metadata_filename metadata_sha <<<"${artifact_info}"

  validate_manifest_path_component "version" "${version}"
  validate_manifest_path_component "database filename" "${db_filename}"
  # The asset may be version-stamped (e.g. ChiaKeySource-2026.06.14.db); it is
  # bundled as ChiaKeySource.db regardless. Only require a .db extension.
  if [[ "${db_filename}" != *.db ]]; then
    echo "Lexicon release database filename must end with .db: ${db_filename}" >&2
    exit 1
  fi
  if [[ -n "${metadata_url}" ]]; then
    validate_manifest_path_component "metadata filename" "${metadata_filename}"
  fi

  if [[ "${db_schema_version}" != "1" ]]; then
    echo "Unsupported lexicon database schema version: ${db_schema_version}" >&2
    exit 1
  fi

  local db_download="${lexicon_dir}/${db_filename}"
  local metadata_download=""
  echo "Downloading lexicon database:"
  echo "  ${db_url}"
  download "${db_download}" "${db_url}"
  verify_sha256 "${db_download}" "${db_sha}"

  if [[ -n "${metadata_url}" ]]; then
    metadata_download="${lexicon_dir}/${metadata_filename}"
    echo "Downloading lexicon metadata:"
    echo "  ${metadata_url}"
    download "${metadata_download}" "${metadata_url}"
    verify_sha256 "${metadata_download}" "${metadata_sha}"
  fi

  run "${LEXICON_INSTALL_SCRIPT}" --validate-db "${db_download}"

  run /bin/rm -rf "${BUILT_RESOURCES}/Databases"
  run /bin/mkdir -p "${BUILT_RESOURCES}/Databases"
  run /bin/cp "${db_download}" "${BUILT_RESOURCES}/Databases/ChiaKeySource.db"
  run /bin/cp "${manifest_file}" "${BUILT_RESOURCES}/Databases/lexicon-manifest.json"
  if [[ -n "${metadata_download}" ]]; then
    run /bin/cp "${metadata_download}" "${BUILT_RESOURCES}/Databases/metadata.json"
  fi

  echo "Bundled ChiaKey lexicon release ${version}."
}

create_product_resources() {
  run /bin/rm -rf "${PRODUCT_RESOURCES_DIR}"
  run /bin/mkdir -p "${PRODUCT_RESOURCES_DIR}"
  run /usr/bin/ditto --norsrc "${INSTALLER_RESOURCES_DIR}" "${PRODUCT_RESOURCES_DIR}"
  run /bin/cp "${LICENSE_FILE}" "${PRODUCT_RESOURCES_DIR}/License.txt"
  for lproj_dir in "${PRODUCT_RESOURCES_DIR}"/*.lproj; do
    if [[ -d "${lproj_dir}" ]]; then
      run /bin/cp "${LICENSE_FILE}" "${lproj_dir}/License.txt"
    fi
  done
}

create_distribution_file() {
  print_command /usr/bin/sed \
    -e "s#__COMPONENT_IDENTIFIER__#${COMPONENT_IDENTIFIER}#g" \
    -e "s#__VERSION__#${VERSION}#g" \
    -e "s#__CLEAN_COMPONENT_PKG_NAME__#${CLEAN_COMPONENT_PKG_NAME}#g" \
    "${INSTALLER_DISTRIBUTION_TEMPLATE}" ">" "${DISTRIBUTION_FILE}"

  /usr/bin/sed \
    -e "s#__COMPONENT_IDENTIFIER__#${COMPONENT_IDENTIFIER}#g" \
    -e "s#__VERSION__#${VERSION}#g" \
    -e "s#__CLEAN_COMPONENT_PKG_NAME__#${CLEAN_COMPONENT_PKG_NAME}#g" \
    "${INSTALLER_DISTRIBUTION_TEMPLATE}" >"${DISTRIBUTION_FILE}"
}

require_lexicon_database_source() {
  if [[ -f "${SMART_MANDARIN_DB}" ||
        "${BUNDLE_LOCAL_LEXICON}" == "1" ||
        "${BUNDLE_RELEASE_LEXICON}" == "1" ]]; then
    return
  fi

  cat >&2 <<EOF
Bundled fallback lexicon database not found:
  ${SMART_MANDARIN_DB}

Release packaging does not rebuild ChiaKeySource.db from raw DataSource files.
By default it downloads the latest ChiaKey-Lexicon GitHub release. If you
explicitly disabled that flow, either:
  - rerun without the local lexicon override, or
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
    --verbose)
      VERBOSE=1
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
      BUNDLE_RELEASE_LEXICON=0
      shift
      ;;
    --local-lexicon)
      require_value "$1" "${2:-}"
      BUNDLE_LOCAL_LEXICON=1
      BUNDLE_RELEASE_LEXICON=0
      LOCAL_LEXICON_DB="$2"
      shift 2
      ;;
    --bundle-release-lexicon)
      BUNDLE_RELEASE_LEXICON=1
      BUNDLE_LOCAL_LEXICON=0
      shift
      ;;
    --lexicon-repo)
      require_value "$1" "${2:-}"
      LEXICON_RELEASE_REPO="$2"
      shift 2
      ;;
    --lexicon-tag)
      require_value "$1" "${2:-}"
      LEXICON_RELEASE_TAG="$2"
      shift 2
      ;;
    --lexicon-manifest-url)
      require_value "$1" "${2:-}"
      LEXICON_RELEASE_MANIFEST_URL="$2"
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
CLEAN_COMPONENT_PKG_NAME="$(basename "${CLEAN_COMPONENT_PKG}")"
DISTRIBUTION_FILE="${WORK_DIR}/Distribution.xml"
PRODUCT_RESOURCES_DIR="${WORK_DIR}/product-resources"

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
  XCODEBUILD_ARGS=(
    /usr/bin/xcodebuild
    -project "${PROJECT}"
    -scheme "${SCHEME}"
    -configuration "${CONFIGURATION}"
    -derivedDataPath "${DERIVED_DATA_PATH}"
    CODE_SIGNING_ALLOWED=NO
    ONLY_ACTIVE_ARCH=YES
  )
  if [[ "${VERBOSE}" == "1" ]]; then
    # Full output and warnings.
    run "${XCODEBUILD_ARGS[@]}" build
  else
    # Quiet: suppress per-file progress, compiler warnings, and benign notes.
    XCODEBUILD_ARGS+=(-quiet GCC_WARN_INHIBIT_ALL_WARNINGS=YES SWIFT_SUPPRESS_WARNINGS=YES)
    "${XCODEBUILD_ARGS[@]}" build 2>&1 | filter_build_noise
  fi
fi

if [[ ! -d "${BUILT_APP}" ]]; then
  echo "Build product not found: ${BUILT_APP}" >&2
  exit 1
fi

run /bin/mkdir -p "${BUILT_RESOURCES}"
run /bin/rm -rf "${BUILT_RESOURCES}/DataTables"
run /usr/bin/ditto --norsrc "${DATA_TABLES_DIR}" "${BUILT_RESOURCES}/DataTables"

require_lexicon_database_source

if [[ "${BUNDLE_RELEASE_LEXICON}" != "1" && -f "${SMART_MANDARIN_DB}" ]]; then
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

copy_legal_notices

if [[ "${BUNDLE_LOCAL_LEXICON}" == "1" ]]; then
  run "${LOCAL_LEXICON_BUNDLE_SCRIPT}" \
    --app "${BUILT_APP}" \
    --source "${LOCAL_LEXICON_DB}" \
    --suppress-signing-note
fi

if [[ "${BUNDLE_RELEASE_LEXICON}" == "1" ]]; then
  copy_release_lexicon
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
  if [[ -z "${INSTALLER_SIGN_IDENTITY}" ]]; then
    PKG_NAME="ChiaKey-${VERSION}-unsigned.pkg"
  else
    PKG_NAME="ChiaKey-${VERSION}.pkg"
  fi
fi

OUTPUT_PKG="${OUTPUT_DIR}/${PKG_NAME}"

run /bin/rm -rf "${STAGE_ROOT}" "${PKG_SCRIPTS_DIR}" "${COMPONENT_PKG}"
run /bin/mkdir -p "${STAGE_ROOT}/Library/Input Methods" "${PKG_SCRIPTS_DIR}" "${OUTPUT_DIR}"
run /usr/bin/ditto --norsrc "${BUILT_APP}" "${STAGE_ROOT}/Library/Input Methods/${APP_NAME}"
run /usr/bin/find "${STAGE_ROOT}" -name "._*" -delete
run /usr/bin/find "${STAGE_ROOT}" -name ".DS_Store" -delete
run /usr/bin/xattr -cr "${STAGE_ROOT}"

run /bin/cp "${INSTALLER_SCRIPTS_DIR}/postinstall" "${PKG_SCRIPTS_DIR}/postinstall"
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

create_product_resources
create_distribution_file

PRODUCTBUILD_ARGS=(
  /usr/bin/productbuild
  --distribution "${DISTRIBUTION_FILE}"
  --resources "${PRODUCT_RESOURCES_DIR}"
  --package-path "${WORK_DIR}"
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
    "${CLEAN_COMPONENT_PKG}" \
    "${DISTRIBUTION_FILE}" \
    "${PRODUCT_RESOURCES_DIR}"
fi

cat <<EOF

Built installer package:
  ${OUTPUT_PKG}

Install target:
  /Library/Input Methods/${APP_NAME}
  or ~/Library/Input Methods/${APP_NAME}, depending on the Installer domain
  selected by the user.

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
