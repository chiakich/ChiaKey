#!/usr/bin/env bash
set -euo pipefail

REPO="akira02/Chiaki-KeyKey-Lexicon"
TAG="2026.06.5"
MANIFEST_URL=""
INSTALL_ROOT="${HOME}/Library/Application Support/Chiaki KeyKey/Lexicons"
DRY_RUN=0
KEEP_DOWNLOADS=0
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PUNCTUATION_CIN=""

for candidate in \
  "${SCRIPT_DIR}/DataTables/bpmf-punctuations.cin" \
  "${SCRIPT_DIR}/../DataTables/bpmf-punctuations.cin" \
  "${SCRIPT_DIR}/../YahooKeyKey-Source-1.1.2528/DataTables/bpmf-punctuations.cin"; do
  if [[ -f "${candidate}" ]]; then
    PUNCTUATION_CIN="${candidate}"
    break
  fi
done

usage() {
  cat <<EOF
Usage: Scripts/install-lexicon-release.sh [options]

Download, verify, and install a Chiaki KeyKey lexicon release into:
  ${INSTALL_ROOT}

Options:
  --repo OWNER/REPO        GitHub repository. Default: ${REPO}
  --tag TAG               Release tag. Default: ${TAG}
  --manifest-url URL      Manifest URL. Overrides --repo/--tag URL composition.
  --install-root PATH     Install root. Default: ${INSTALL_ROOT}
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

if [[ -z "${MANIFEST_URL}" ]]; then
  MANIFEST_URL="https://github.com/${REPO}/releases/download/${TAG}/lexicon-manifest.json"
fi

case "${INSTALL_ROOT}" in
  "${HOME}"/Library/Application\ Support/Chiaki\ KeyKey/Lexicons*) ;;
  *)
    echo "Refusing to install outside Chiaki KeyKey Application Support: ${INSTALL_ROOT}" >&2
    exit 1
    ;;
esac

TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/ChiakiKeyKeyLexicon.XXXXXX")"
if [[ "${KEEP_DOWNLOADS}" != "1" ]]; then
  trap 'rm -rf "${TMP_DIR}"' EXIT
else
  echo "Keeping downloads in: ${TMP_DIR}"
fi

MANIFEST_FILE="${TMP_DIR}/lexicon-manifest.json"

echo "Downloading manifest:"
echo "  ${MANIFEST_URL}"
/usr/bin/curl -fL --retry 3 --output "${MANIFEST_FILE}" "${MANIFEST_URL}"

ARTIFACT_INFO="$(
  /usr/bin/ruby -rjson - "${MANIFEST_FILE}" <<'RUBY'
manifest_path = ARGV.fetch(0)
manifest = JSON.parse(File.read(manifest_path))

db = manifest.fetch("artifacts").find { |artifact| artifact["kind"] == "keykey-source-db" }
metadata = manifest.fetch("artifacts").find { |artifact| artifact["kind"] == "metadata" }

abort "manifest does not contain a keykey-source-db artifact" unless db

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

if [[ "${DB_SCHEMA_VERSION}" != "1" ]]; then
  echo "Unsupported database schema version: ${DB_SCHEMA_VERSION}" >&2
  exit 1
fi

DB_DOWNLOAD="${TMP_DIR}/${DB_FILENAME}"
METADATA_DOWNLOAD=""

echo "Downloading database:"
echo "  ${DB_URL}"
/usr/bin/curl -fL --retry 3 --output "${DB_DOWNLOAD}" "${DB_URL}"

if [[ -n "${METADATA_URL}" ]]; then
  METADATA_DOWNLOAD="${TMP_DIR}/${METADATA_FILENAME}"
  echo "Downloading metadata:"
  echo "  ${METADATA_URL}"
  /usr/bin/curl -fL --retry 3 --output "${METADATA_DOWNLOAD}" "${METADATA_URL}"
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

validate_db_table() {
  local table="$1"
  local found

  found="$(/usr/bin/sqlite3 "${DB_DOWNLOAD}" "SELECT name FROM sqlite_master WHERE type='table' AND name='${table}';")"
  if [[ "${found}" != "${table}" ]]; then
    echo "Database is missing required table: ${table}" >&2
    exit 1
  fi
}

validate_db_table "cooked_information"
validate_db_table "prepopulated_service_data"
validate_db_table "unigrams"
validate_db_table "bigrams"

import_punctuation_rows() {
  local db_path="$1"

  if [[ -z "${PUNCTUATION_CIN}" || ! -f "${PUNCTUATION_CIN}" ]]; then
    echo "Warning: bpmf-punctuations.cin was not found; skipping punctuation import." >&2
    return
  fi

  echo "Importing punctuation table:"
  echo "  ${PUNCTUATION_CIN}"

  /usr/bin/ruby -rdigest -ropen3 - "${db_path}" "${PUNCTUATION_CIN}" <<'RUBY'
db_path = ARGV.fetch(0)
punctuation_path = ARGV.fetch(1)

def sql(value)
  "'#{value.to_s.gsub("'", "''")}'"
end

rows = []
row_set = {}
seen = 0
inside_chardef = false

File.foreach(punctuation_path, chomp: true) do |line|
  line = line.sub(/\s+#.*$/, "").strip
  next if line.empty? || line.start_with?("#")

  if line =~ /^%chardef\s+begin/i
    inside_chardef = true
    next
  end

  if line =~ /^%chardef\s+end/i
    inside_chardef = false
    next
  end

  next unless inside_chardef

  key, value = line.split(/\s+/, 2)
  seen += 1
  next unless key&.start_with?("_") && value && !value.empty?

  row_key = [key, value]
  next if row_set[row_key]

  row_set[row_key] = true
  rows << [key, value]
end

offsets_by_qstring = Hash.new(0)
source = "YahooKeyKey-Source-1.1.2528/DataTables/bpmf-punctuations.cin"
sha256 = Digest::SHA256.file(punctuation_path).hexdigest
skipped = seen - rows.length

statements = []
statements << "BEGIN;"
statements << "DELETE FROM unigrams WHERE qstring GLOB '_punctuation_*' OR qstring GLOB '_ctrl_*';"
statements << "DELETE FROM 'Mandarin-bpmf-cin' WHERE key GLOB '_punctuation_*' OR key GLOB '_ctrl_*';"

rows.each do |qstring, value|
  offset = offsets_by_qstring[qstring]
  offsets_by_qstring[qstring] += 1
  probability = 0.0 - (offset * 0.001)
  statements << "INSERT INTO unigrams VALUES(#{sql(qstring)}, #{sql(value)}, #{probability}, 0.0);"
  statements << "INSERT INTO 'Mandarin-bpmf-cin' VALUES(#{sql(qstring)}, #{sql(value)});"
end

statements << "UPDATE chiaki_db_metadata SET value = (SELECT COUNT(*) FROM unigrams) WHERE key = 'unigram_count';"
statements << "UPDATE chiaki_db_metadata SET value = (SELECT COUNT(*) FROM 'Mandarin-bpmf-cin') WHERE key = 'candidate_count';"
statements << "DELETE FROM chiaki_db_sources WHERE source = #{sql(source)};"
statements << "INSERT INTO chiaki_db_sources VALUES(#{sql(source)}, 'punctuation', #{sql(sha256)}, #{seen}, #{rows.length}, #{skipped});"
statements << "COMMIT;"

stdout, stderr, status = Open3.capture3("/usr/bin/sqlite3", db_path, stdin_data: statements.join("\n"))
unless status.success?
  warn stderr
  warn stdout
  exit status.exitstatus || 1
end

puts "Imported #{rows.length} punctuation rows."
RUBY
}

import_punctuation_rows "${DB_DOWNLOAD}"

VERSION_DIR="${INSTALL_ROOT}/versions/${VERSION}"
ACTIVE_LINK="${INSTALL_ROOT}/active"
TMP_LINK="${INSTALL_ROOT}/active.tmp.$$"

run /bin/mkdir -p "${VERSION_DIR}"
run /bin/cp "${DB_DOWNLOAD}" "${VERSION_DIR}/KeyKeySource.db"
run /bin/cp "${MANIFEST_FILE}" "${VERSION_DIR}/lexicon-manifest.json"
if [[ -n "${METADATA_DOWNLOAD}" ]]; then
  run /bin/cp "${METADATA_DOWNLOAD}" "${VERSION_DIR}/metadata.json"
fi

if [[ "${DRY_RUN}" != "1" ]]; then
  /bin/rm -rf "${TMP_LINK}"
  /bin/ln -s "${VERSION_DIR}" "${TMP_LINK}"
  /bin/rm -rf "${ACTIVE_LINK}"
  /bin/mv "${TMP_LINK}" "${ACTIVE_LINK}"
else
  print_command /bin/ln -s "${VERSION_DIR}" "${TMP_LINK}"
  print_command /bin/rm -rf "${ACTIVE_LINK}"
  print_command /bin/mv "${TMP_LINK}" "${ACTIVE_LINK}"
fi

if [[ "${DRY_RUN}" == "1" ]]; then
  cat <<EOF

Dry run complete for Chiaki KeyKey lexicon ${VERSION}.

Planned active lexicon:
  ${ACTIVE_LINK}/KeyKeySource.db
EOF
else
  cat <<EOF

Installed Chiaki KeyKey lexicon ${VERSION}.

Active lexicon:
  ${ACTIVE_LINK}/KeyKeySource.db

Switch away from and back to Chiaki KeyKey, or reinstall/relaunch the input
method, so the runtime can reopen the database.
EOF
fi
