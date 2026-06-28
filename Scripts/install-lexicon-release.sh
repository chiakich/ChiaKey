#!/usr/bin/env bash
set -euo pipefail

REPO="akira02/ChiaKey-Lexicon"
TAG=""
MANIFEST_URL=""
INSTALL_ROOT="${HOME}/Library/Application Support/ChiaKey/Lexicons"
DB_INSTALL_FILENAME="ChiaKeySource.db"
DB_RELEASE_FILENAME="ChiaKeySource.db"
DRY_RUN=0
KEEP_DOWNLOADS=0
SKIP_CURRENT=0
MIN_RELEASE_AGE_DAYS=""
VALIDATE_DB_PATH=""
TMP_DIR=""
declare -a VALIDATION_TMP_DIRS=()

usage() {
  cat <<EOF
Usage: Scripts/install-lexicon-release.sh [options]

Download, verify, and install a ChiaKey lexicon release into:
  ${INSTALL_ROOT}

Options:
  --repo OWNER/REPO        GitHub repository. Default: ${REPO}
  --tag TAG               Release tag. Default: latest release.
  --manifest-url URL      Manifest URL. Overrides --repo/--tag URL composition.
  --install-root PATH     Install root. Default: ${INSTALL_ROOT}
  --skip-current          Do nothing when active lexicon is same or newer.
  --min-release-age-days N
                          Only install latest release after it is at least N days old.
  --validate-db PATH      Validate an existing ChiaKeySource.db and exit.
  --dry-run               Print install actions without writing Application Support.
  --keep-downloads        Keep the temporary download directory.
  -h, --help              Show this help.
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

cleanup() {
  if [[ "${KEEP_DOWNLOADS}" != "1" && -n "${TMP_DIR}" ]]; then
    rm -rf "${TMP_DIR}"
  fi

  if (( ${#VALIDATION_TMP_DIRS[@]} > 0 )); then
    for dir in "${VALIDATION_TMP_DIRS[@]}"; do
      rm -rf "${dir}"
    done
  fi
}

trap cleanup EXIT

curl() {
  /usr/bin/curl -fL --retry 3 --silent --show-error \
    --header "User-Agent: ChiaKey Lexicon Installer" \
    "$@"
}

current_lexicon_version() {
  /usr/bin/ruby -rjson - "${INSTALL_ROOT}/active/metadata.json" \
    "${INSTALL_ROOT}/active/lexicon-manifest.json" <<'RUBY' 2>/dev/null || true
ARGV.each do |path|
  next unless File.file?(path)
  data = JSON.parse(File.read(path))
  version = data["version"]
  if version && !version.empty?
    puts version
    exit
  end
end
RUBY
}

compare_versions() {
  /usr/bin/ruby - "$1" "$2" <<'RUBY'
left = ARGV.fetch(0).split(".").map(&:to_i)
right = ARGV.fetch(1).split(".").map(&:to_i)
length = [left.length, right.length].max
left.fill(0, left.length...length)
right.fill(0, right.length...length)
puts(left <=> right)
RUBY
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

validate_database_health() {
  local db_path="$1"
  local validation_tmp_dir

  validation_tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/ChiaKeyLexiconValidate.XXXXXX")"
  VALIDATION_TMP_DIRS+=("${validation_tmp_dir}")

  validate_db_table() {
    local table="$1"
    local found

    found="$(/usr/bin/sqlite3 "${db_path}" "SELECT name FROM sqlite_master WHERE type='table' AND name='${table}';")"
    if [[ "${found}" != "${table}" ]]; then
      echo "Database is missing required table: ${table}" >&2
      exit 1
    fi
  }

  validate_db_scalar() {
    local description="$1"
    local sql="$2"
    local expected="$3"
    local actual

    actual="$(/usr/bin/sqlite3 "${db_path}" "${sql}")"
    if [[ "${actual}" != "${expected}" ]]; then
      echo "Database validation failed: ${description}" >&2
      echo "  expected: ${expected}" >&2
      echo "  actual:   ${actual:-<empty>}" >&2
      exit 1
    fi
  }

  validate_db_minimum_count() {
    local description="$1"
    local sql="$2"
    local minimum="$3"
    local actual

    actual="$(/usr/bin/sqlite3 "${db_path}" "${sql}")"
    if ! [[ "${actual}" =~ ^[0-9]+$ ]] || (( actual < minimum )); then
      echo "Database validation failed: ${description}" >&2
      echo "  expected at least: ${minimum}" >&2
      echo "  actual:            ${actual:-<empty>}" >&2
      exit 1
    fi
  }

  validate_canned_messages_plist() {
    local plist_path="${validation_tmp_dir}/canned_messages.plist"
    /usr/bin/sqlite3 "${db_path}" \
      "SELECT value FROM prepopulated_service_data WHERE key = 'canned_messages' LIMIT 1;" \
      > "${plist_path}"

    if ! /usr/bin/plutil -lint "${plist_path}" >/dev/null; then
      echo "Validation failed: canned_messages is not a valid plist" >&2
      exit 1
    fi

    local category_count
    category_count="$(/usr/bin/ruby - "${plist_path}" <<'RUBY'
require "rexml/document"

document = REXML::Document.new(File.read(ARGV.fetch(0)))
root_dictionary = document.root&.elements&.[]("dict")
abort "missing plist root dictionary" unless root_dictionary

children = root_dictionary.elements.to_a
canned_messages = nil
children.each_with_index do |element, index|
  next unless element.name == "key" && element.text == "CannedMessages"
  canned_messages = children[(index + 1)..]&.find { |candidate| candidate.name == "array" }
  break
end

abort "missing CannedMessages array" unless canned_messages
puts canned_messages.elements.to_a.count { |element| element.name == "dict" }
RUBY
)"

    echo "  - canned messages categories: ${category_count}"
    if (( category_count < 1 )); then
      echo "Validation failed: canned_messages has no categories" >&2
      exit 1
    fi
  }

  echo "Validating database health:"
  validate_db_scalar "SQLite integrity check" "PRAGMA integrity_check;" "ok"

  validate_db_table "cooked_information"
  validate_db_table "prepopulated_service_data"
  validate_db_table "unigrams"
  validate_db_table "bigrams"
  validate_db_table "Mandarin-bpmf-cin"
  validate_db_table "chiaki_db_metadata"
  validate_db_table "chiaki_db_sources"

  validate_db_minimum_count "unigrams table has enough rows" "SELECT COUNT(*) FROM unigrams;" 1000
  validate_db_minimum_count "Mandarin-bpmf-cin table has enough rows" "SELECT COUNT(*) FROM 'Mandarin-bpmf-cin';" 1000
  validate_db_minimum_count "punctuation list unigrams are present" "SELECT COUNT(*) FROM unigrams WHERE qstring = '_punctuation_list';" 50
  validate_db_minimum_count "punctuation list candidates are present" "SELECT COUNT(*) FROM 'Mandarin-bpmf-cin' WHERE key = '_punctuation_list';" 50
  validate_db_minimum_count "prepopulated canned messages are present" "SELECT COUNT(*) FROM prepopulated_service_data WHERE key = 'canned_messages' AND LENGTH(value) > 1000;" 1
  validate_db_minimum_count "prepopulated canned messages timestamp is present" "SELECT COUNT(*) FROM prepopulated_service_data WHERE key = 'canned_messages_timestamp' AND CAST(value AS INTEGER) > 0;" 1
  validate_canned_messages_plist
  validate_db_scalar "metadata schema_version" "SELECT value FROM chiaki_db_metadata WHERE key = 'schema_version';" "1"
  validate_db_scalar "cooked_information version" "SELECT COUNT(*) FROM cooked_information WHERE key = 'version' AND value != '';" "1"
  validate_db_scalar "Shift+, punctuation unigram" "SELECT current FROM unigrams WHERE qstring = '_punctuation_<' ORDER BY probability DESC, current LIMIT 1;" "，"
  validate_db_scalar "Standard Shift+, punctuation unigram" "SELECT current FROM unigrams WHERE qstring = '_punctuation_Standard_<' ORDER BY probability DESC, current LIMIT 1;" "，"
  validate_db_scalar "Shift+, punctuation candidate" "SELECT value FROM 'Mandarin-bpmf-cin' WHERE key = '_punctuation_<' ORDER BY value LIMIT 1;" "，"
  validate_db_scalar "Standard Shift+, punctuation candidate" "SELECT value FROM 'Mandarin-bpmf-cin' WHERE key = '_punctuation_Standard_<' ORDER BY value LIMIT 1;" "，"
  echo "Database health validation passed."
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo)
      REPO="${2:-}"
      shift 2
      ;;
    --tag)
      TAG="${2:-}"
      shift 2
      ;;
    --manifest-url)
      MANIFEST_URL="${2:-}"
      shift 2
      ;;
    --install-root)
      INSTALL_ROOT="${2:-}"
      shift 2
      ;;
    --skip-current)
      SKIP_CURRENT=1
      shift
      ;;
    --min-release-age-days)
      MIN_RELEASE_AGE_DAYS="${2:-}"
      shift 2
      ;;
    --validate-db)
      VALIDATE_DB_PATH="${2:-}"
      shift 2
      ;;
    --dry-run)
      DRY_RUN=1
      shift
      ;;
    --keep-downloads)
      KEEP_DOWNLOADS=1
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

if [[ -n "${MIN_RELEASE_AGE_DAYS}" ]] &&
   ! [[ "${MIN_RELEASE_AGE_DAYS}" =~ ^[0-9]+$ ]]; then
  echo "--min-release-age-days must be a non-negative integer" >&2
  exit 2
fi

case "${INSTALL_ROOT}" in
  "${HOME}"/Library/Application\ Support/ChiaKey/Lexicons*) ;;
  *)
    echo "Refusing to install outside ChiaKey Application Support: ${INSTALL_ROOT}" >&2
    exit 1
    ;;
esac

if [[ -n "${VALIDATE_DB_PATH}" ]]; then
  validate_database_health "${VALIDATE_DB_PATH}"
  exit 0
fi

TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/ChiaKeyLexicon.XXXXXX")"
if [[ "${KEEP_DOWNLOADS}" == "1" ]]; then
  echo "Keeping downloads in: ${TMP_DIR}"
fi

MANIFEST_FILE="${TMP_DIR}/lexicon-manifest.json"

if [[ -z "${MANIFEST_URL}" &&
      ( -n "${MIN_RELEASE_AGE_DAYS}" || "${SKIP_CURRENT}" == "1" ) ]]; then
  RELEASE_INFO_FILE="${TMP_DIR}/latest-release.json"
  RELEASE_API_URL="https://api.github.com/repos/${REPO}/releases/latest"

  echo "Checking latest release:"
  echo "  ${RELEASE_API_URL}"
  curl --header "Accept: application/vnd.github+json" \
    --output "${RELEASE_INFO_FILE}" "${RELEASE_API_URL}"

  RELEASE_INFO="$(
    /usr/bin/ruby -rjson -rtime - "${RELEASE_INFO_FILE}" "${MIN_RELEASE_AGE_DAYS:-}" <<'RUBY'
release_path = ARGV.fetch(0)
min_age_days = ARGV.fetch(1)
release = JSON.parse(File.read(release_path))
tag = release.fetch("tag_name")
published_at = Time.parse(release.fetch("published_at"))
age_seconds = Time.now - published_at
min_age_seconds = min_age_days.empty? ? 0 : min_age_days.to_i * 24 * 60 * 60
fields = [
  tag,
  published_at.utc.iso8601,
  age_seconds.to_i,
  min_age_seconds.to_i,
  age_seconds >= min_age_seconds ? "ready" : "too_new"
]
puts fields.join("\t")
RUBY
  )"

  IFS=$'\t' read -r RELEASE_TAG RELEASE_PUBLISHED_AT RELEASE_AGE_SECONDS RELEASE_MIN_AGE_SECONDS RELEASE_AGE_STATUS <<<"${RELEASE_INFO}"
  if [[ -z "${TAG}" ]]; then
    TAG="${RELEASE_TAG}"
  fi

  if [[ -n "${MIN_RELEASE_AGE_DAYS}" &&
        "${RELEASE_AGE_STATUS}" != "ready" ]]; then
    cat <<EOF
Skipping ChiaKey lexicon ${RELEASE_TAG}: release is newer than ${MIN_RELEASE_AGE_DAYS} days.
Published at: ${RELEASE_PUBLISHED_AT}
Age seconds:  ${RELEASE_AGE_SECONDS}
Required:     ${RELEASE_MIN_AGE_SECONDS}
EOF
    exit 0
  fi
fi

if [[ "${SKIP_CURRENT}" == "1" && -n "${TAG}" ]]; then
  CURRENT_VERSION="$(current_lexicon_version)"
  if [[ -n "${CURRENT_VERSION}" ]] &&
     [[ "$(compare_versions "${CURRENT_VERSION}" "${TAG}")" != "-1" ]]; then
    echo "Skipping ChiaKey lexicon ${TAG}: active lexicon ${CURRENT_VERSION} is current."
    exit 0
  fi
fi

if [[ -z "${MANIFEST_URL}" ]]; then
  if [[ -n "${TAG}" ]]; then
    MANIFEST_URL="https://github.com/${REPO}/releases/download/${TAG}/lexicon-manifest.json"
  else
    MANIFEST_URL="https://github.com/${REPO}/releases/latest/download/lexicon-manifest.json"
  fi
fi

echo "Downloading manifest:"
echo "  ${MANIFEST_URL}"
curl --output "${MANIFEST_FILE}" "${MANIFEST_URL}"

ARTIFACT_INFO="$(
  /usr/bin/ruby -rjson - "${MANIFEST_FILE}" <<'RUBY'
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

IFS=$'\t' read -r VERSION DB_SCHEMA_VERSION DB_URL DB_FILENAME DB_SHA METADATA_URL METADATA_FILENAME METADATA_SHA <<<"${ARTIFACT_INFO}"

validate_manifest_path_component "version" "${VERSION}"
validate_manifest_path_component "database filename" "${DB_FILENAME}"
if [[ "${DB_FILENAME}" != "${DB_RELEASE_FILENAME}" ]]; then
  echo "Lexicon release database filename must be ${DB_RELEASE_FILENAME}: ${DB_FILENAME}" >&2
  exit 1
fi
if [[ -n "${METADATA_URL}" ]]; then
  validate_manifest_path_component "metadata filename" "${METADATA_FILENAME}"
fi

if [[ "${DB_SCHEMA_VERSION}" != "1" ]]; then
  echo "Unsupported database schema version: ${DB_SCHEMA_VERSION}" >&2
  exit 1
fi

DB_DOWNLOAD="${TMP_DIR}/${DB_FILENAME}"
METADATA_DOWNLOAD=""

echo "Downloading database:"
echo "  ${DB_URL}"
curl --output "${DB_DOWNLOAD}" "${DB_URL}"

if [[ -n "${METADATA_URL}" ]]; then
  METADATA_DOWNLOAD="${TMP_DIR}/${METADATA_FILENAME}"
  echo "Downloading metadata:"
  echo "  ${METADATA_URL}"
  curl --output "${METADATA_DOWNLOAD}" "${METADATA_URL}"
fi

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

verify_sha256 "${DB_DOWNLOAD}" "${DB_SHA}"
if [[ -n "${METADATA_DOWNLOAD}" ]]; then
  verify_sha256 "${METADATA_DOWNLOAD}" "${METADATA_SHA}"
fi

validate_database_health "${DB_DOWNLOAD}"

VERSION_DIR="${INSTALL_ROOT}/versions/${VERSION}"
ACTIVE_LINK="${INSTALL_ROOT}/active"

run /bin/mkdir -p "${VERSION_DIR}"
run /bin/cp "${DB_DOWNLOAD}" "${VERSION_DIR}/${DB_INSTALL_FILENAME}"
run /bin/cp "${MANIFEST_FILE}" "${VERSION_DIR}/lexicon-manifest.json"
if [[ -n "${METADATA_DOWNLOAD}" ]]; then
  run /bin/cp "${METADATA_DOWNLOAD}" "${VERSION_DIR}/metadata.json"
fi

if [[ "${DRY_RUN}" != "1" ]]; then
  /bin/ln -sfn "${VERSION_DIR}" "${ACTIVE_LINK}"
else
  print_command /bin/ln -sfn "${VERSION_DIR}" "${ACTIVE_LINK}"
fi

if [[ "${DRY_RUN}" == "1" ]]; then
  cat <<EOF

Dry run complete for ChiaKey lexicon ${VERSION}.

Planned active lexicon:
  ${ACTIVE_LINK}/${DB_INSTALL_FILENAME}
EOF
else
  cat <<EOF

Installed ChiaKey lexicon ${VERSION}.

Active lexicon:
  ${ACTIVE_LINK}/${DB_INSTALL_FILENAME}

Switch away from and back to ChiaKey, or reinstall/relaunch the input
method, so the runtime can reopen the database.
EOF
fi
