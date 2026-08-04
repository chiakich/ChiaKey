#!/usr/bin/env bash
set -euo pipefail

REPO="chiakich/ChiaKey-Lexicon"
TAG=""
MANIFEST_URL=""
INSTALL_ROOT="${HOME}/Library/Application Support/ChiaKey/Lexicons"
DB_INSTALL_FILENAME="ChiaKeySource.db"
DRY_RUN=0
KEEP_DOWNLOADS=0
SKIP_CURRENT=0
MIN_RELEASE_AGE_DAYS=""
VALIDATE_DB_PATH=""
PRUNE_SUPERSEDED=0
ROLLBACK=0
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
  --prune-superseded      Delete every installed version except the active one,
                          then clear the pending-verification marker, and exit.
  --rollback              Point the active lexicon back at the version the
                          pending one replaced, delete it, and exit.
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

lexicon_version_in_dir() {
  /usr/bin/ruby -rjson - "$1/metadata.json" \
    "$1/lexicon-manifest.json" <<'RUBY' 2>/dev/null || true
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

current_lexicon_version() {
  lexicon_version_in_dir "${INSTALL_ROOT}/active"
}

pending_verification_file() {
  echo "${INSTALL_ROOT}/pending-verification"
}

resolve_path() {
  /usr/bin/ruby -e 'begin; puts File.realpath(ARGV[0]); rescue; end' "$1"
}

# Line 1 is the version awaiting verification, line 2 the one it replaced (empty
# on a first install). Line 2 is what a rollback needs, so a single-line marker
# written by an older build simply has nowhere to go back to.
pending_verification_version() {
  /usr/bin/sed -n '1p' "$(pending_verification_file)" 2>/dev/null || true
}

pending_verification_previous() {
  /usr/bin/sed -n '2p' "$(pending_verification_file)" 2>/dev/null || true
}

# Restores the lexicon the failed one replaced. Repointing the symlink rather
# than letting the runtime pick a different file keeps one source of truth: the
# version the Preferences app shows, the version --skip-current compares
# against, and the database actually in use stay the same thing.
rollback_pending_version() {
  local marker pending previous previous_dir active_dir
  marker="$(pending_verification_file)"

  if [[ ! -f "${marker}" ]]; then
    echo "No lexicon awaiting verification; nothing to roll back." >&2
    exit 1
  fi

  active_dir="$(resolve_path "${INSTALL_ROOT}/active")"

  # Only the version this marker is about may be rolled back and deleted. If
  # active has moved on since, the marker is stale and acting on it would throw
  # away a lexicon nobody reported a problem with.
  pending="$(pending_verification_version)"
  if [[ -z "${active_dir}" || "$(basename "${active_dir}")" != "${pending}" ]]; then
    echo "Active lexicon is no longer ${pending}; clearing the stale marker." >&2
    run /bin/rm -f "${marker}"
    exit 0
  fi

  previous="$(pending_verification_previous)"
  previous_dir=""
  [[ -n "${previous}" ]] && previous_dir="${INSTALL_ROOT}/versions/${previous}"

  if [[ -n "${previous_dir}" && -d "${previous_dir}" ]]; then
    if [[ "$(resolve_path "${previous_dir}")" == "${active_dir}" ]]; then
      echo "Active lexicon is already ${previous}; clearing the marker." >&2
      run /bin/rm -f "${marker}"
      exit 0
    fi

    run /bin/ln -sfn "${previous_dir}" "${INSTALL_ROOT}/active"
    echo "Rolled the active lexicon back to ${previous}."
  else
    # Nothing to go back to: drop the symlink so the runtime uses the bundled
    # database and, just as importantly, so --skip-current stops reading a
    # version number the runtime could never load and skipping every update.
    run /bin/rm -f "${INSTALL_ROOT}/active"
    echo "No previous lexicon to restore; cleared the active symlink."
  fi

  # The failed version is not a rollback target for anyone, and leaving it would
  # keep it in the way of the next install's comparison.
  if [[ -n "${active_dir}" && -d "${active_dir}" ]]; then
    run /bin/rm -rf "${active_dir}"
  fi

  run /bin/rm -f "${marker}"
}

# Old versions exist for one reason: to fall back to if the newly installed one
# turns out not to load. The IME calls this once it has actually opened the
# active lexicon, at which point nothing else is worth the disk.
prune_superseded_versions() {
  local versions_dir="${INSTALL_ROOT}/versions"
  local active_dir dir version

  active_dir="$(resolve_path "${INSTALL_ROOT}/active")"
  if [[ -z "${active_dir}" || ! -d "${active_dir}" ]]; then
    echo "No active lexicon to verify against; keeping every version." >&2
    exit 1
  fi

  if [[ -d "${versions_dir}" ]]; then
    for dir in "${versions_dir}"/*/; do
      dir="${dir%/}"
      [[ -d "${dir}" ]] || continue
      [[ "$(resolve_path "${dir}")" == "${active_dir}" ]] && continue

      # Locally built lexicons are not re-downloadable, so they are not ours
      # to delete; only releases this script installed are.
      version="$(lexicon_version_in_dir "${dir}")"
      [[ "${version}" == "dev" ]] && continue

      run /bin/rm -rf "${dir}"
    done
  fi

  if [[ "${DRY_RUN}" != "1" ]]; then
    rm -f "$(pending_verification_file)"
  fi

  echo "Pruned superseded lexicons; active version kept:"
  echo "  ${active_dir}"
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
    --prune-superseded)
      PRUNE_SUPERSEDED=1
      shift
      ;;
    --rollback)
      ROLLBACK=1
      shift
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

if [[ "${PRUNE_SUPERSEDED}" == "1" ]]; then
  prune_superseded_versions
  exit 0
fi

if [[ "${ROLLBACK}" == "1" ]]; then
  rollback_pending_version
  exit 0
fi

TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/ChiaKeyLexicon.XXXXXX")"
if [[ "${KEEP_DOWNLOADS}" == "1" ]]; then
  echo "Keeping downloads in: ${TMP_DIR}"
fi

MANIFEST_FILE="${TMP_DIR}/lexicon-manifest.json"

R2_LEXICON_MANIFEST_URL="https://cdn.chiaki.ch/chiakey/lexicon/lexicon-manifest.json"

# R2 mirror first: the GitHub API's unauthenticated 60/hour limit is per IP,
# shared by everyone behind one NAT, and this check now runs hourly per
# machine. GitHub stays as the fallback so a CDN outage or regional block
# cannot strand anyone on an old lexicon. An explicit --tag always names a
# specific GitHub release directly, since the R2 mirror only ever holds the
# single latest one.
if [[ -z "${MANIFEST_URL}" ]]; then
  if [[ -n "${TAG}" ]]; then
    MANIFEST_URL="https://github.com/${REPO}/releases/download/${TAG}/lexicon-manifest.json"
  elif curl --fail --output "${MANIFEST_FILE}" "${R2_LEXICON_MANIFEST_URL}" 2>/dev/null; then
    echo "Using manifest mirror:"
    echo "  ${R2_LEXICON_MANIFEST_URL}"
    MANIFEST_URL="${R2_LEXICON_MANIFEST_URL}"
  else
    MANIFEST_URL="https://github.com/${REPO}/releases/latest/download/lexicon-manifest.json"
  fi
fi

if [[ ! -s "${MANIFEST_FILE}" ]]; then
  echo "Downloading manifest:"
  echo "  ${MANIFEST_URL}"
  curl --output "${MANIFEST_FILE}" "${MANIFEST_URL}"
fi

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
  metadata&.fetch("sha256", ""),
  manifest.fetch("generated_at", "")
]

puts fields.join("\t")
RUBY
)"

IFS=$'\t' read -r VERSION DB_SCHEMA_VERSION DB_URL DB_FILENAME DB_SHA METADATA_URL METADATA_FILENAME METADATA_SHA GENERATED_AT <<<"${ARTIFACT_INFO}"

if [[ -z "${TAG}" ]]; then
  TAG="${VERSION}"
fi

# Age and skip-current gating now reads off the manifest we already have
# (whichever source it came from) instead of a separate GitHub API call.
if [[ -n "${MIN_RELEASE_AGE_DAYS}" ]]; then
  AGE_STATUS="$(
    /usr/bin/ruby -rtime - "${GENERATED_AT}" "${MIN_RELEASE_AGE_DAYS}" <<'RUBY'
generated_at = ARGV.fetch(0)
min_age_days = ARGV.fetch(1).to_i
if generated_at.empty?
  puts "ready"
else
  age_seconds = Time.now - Time.parse(generated_at)
  puts(age_seconds >= min_age_days * 24 * 60 * 60 ? "ready" : "too_new")
end
RUBY
  )"

  if [[ "${AGE_STATUS}" != "ready" ]]; then
    cat <<EOF
Skipping ChiaKey lexicon ${VERSION}: release is newer than ${MIN_RELEASE_AGE_DAYS} days.
Generated at: ${GENERATED_AT}
EOF
    exit 0
  fi
fi

if [[ "${SKIP_CURRENT}" == "1" ]]; then
  CURRENT_VERSION="$(current_lexicon_version)"
  if [[ -n "${CURRENT_VERSION}" ]] &&
     [[ "$(compare_versions "${CURRENT_VERSION}" "${VERSION}")" != "-1" ]]; then
    echo "Skipping ChiaKey lexicon ${VERSION}: active lexicon ${CURRENT_VERSION} is current."
    exit 0
  fi
fi

validate_manifest_path_component "version" "${VERSION}"
validate_manifest_path_component "database filename" "${DB_FILENAME}"
if [[ "${DB_FILENAME}" != *.db ]]; then
  echo "Lexicon release database filename must end with .db: ${DB_FILENAME}" >&2
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

PREVIOUS_ACTIVE_DIR="$(resolve_path "${ACTIVE_LINK}")"
PREVIOUS_ACTIVE=""
if [[ -n "${PREVIOUS_ACTIVE_DIR}" && "${PREVIOUS_ACTIVE_DIR}" != "${VERSION_DIR}" ]]; then
  PREVIOUS_ACTIVE="$(basename "${PREVIOUS_ACTIVE_DIR}")"
fi

if [[ "${DRY_RUN}" != "1" ]]; then
  /bin/ln -sfn "${VERSION_DIR}" "${ACTIVE_LINK}"
  # The previous version stays on disk until the IME reports it opened this
  # one; --prune-superseded then clears both it and this marker, and --rollback
  # uses the second line to put the previous one back.
  printf '%s\n%s\n' "${VERSION}" "${PREVIOUS_ACTIVE}" \
    > "$(pending_verification_file)"
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
