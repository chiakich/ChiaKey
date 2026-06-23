#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VALIDATOR="${ROOT_DIR}/Scripts/install-lexicon-release.sh"
BUNDLED_DB="${ROOT_DIR}/ChiaKey-Source/Distributions/Takao/CookedDatabase/ChiaKeySource.db"
ACTIVE_DB="${HOME}/Library/Application Support/ChiaKey/Lexicons/active/ChiaKeySource.db"

usage() {
  cat <<EOF
Usage: Scripts/test-lexicon-smoke.sh [ChiaKeySource.db ...]

Validate ChiaKey lexicon databases without downloading or installing anything.
When no path is provided, this checks the repo-bundled DB and the active user
lexicon if it exists.
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

declare -a db_paths=()

if [[ $# -gt 0 ]]; then
  db_paths=("$@")
else
  if [[ -f "${BUNDLED_DB}" ]]; then
    db_paths+=("${BUNDLED_DB}")
  fi
  if [[ -f "${ACTIVE_DB}" && "${ACTIVE_DB}" != "${BUNDLED_DB}" ]]; then
    db_paths+=("${ACTIVE_DB}")
  fi
fi

if [[ "${#db_paths[@]}" -eq 0 ]]; then
  echo "No ChiaKeySource.db files found to validate." >&2
  exit 1
fi

for db_path in "${db_paths[@]}"; do
  if [[ ! -f "${db_path}" ]]; then
    echo "Database not found: ${db_path}" >&2
    exit 1
  fi

  echo "==> ${db_path}"
  "${VALIDATOR}" --validate-db "${db_path}"
done

echo "Lexicon smoke tests passed."
