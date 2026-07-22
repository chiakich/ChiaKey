#!/usr/bin/env bash
set -uo pipefail

APP_NAME="ChiaKey"
LEGACY_APP_NAME="千秋輸入法"
APP="${HOME}/Library/Input Methods/${APP_NAME}.app"
LEGACY_APP="${HOME}/Library/Input Methods/${LEGACY_APP_NAME}.app"
APP_SUPPORT_DIR="${HOME}/Library/Application Support/ChiaKey"
CACHE_DIR="${HOME}/Library/Caches/com.chiakey.ChiaKey"
UPDATE_LOCK="/var/tmp/com.chiakey.ChiaKey.update.lock"

# Written directly by the preferences app (TakaoHelper), so they are plain
# files rather than cfprefsd-managed domains.
PLIST_GLOB="${HOME}/Library/Preferences/com.chiakey.ChiaKey"

DEFAULTS_DOMAINS=(
  com.chiakey.inputmethod.ChiaKey
  com.chiakey.inputmethod.ChiaKey.Preferences
  com.chiakey.inputmethod.ChiaKey.PhraseEditor
)

PKG_IDENTIFIERS=(
  com.chiakey.inputmethod.ChiaKey.component
  com.chiakey.inputmethod.ChiaKey.pkg
)

PURGE=0
DRY_RUN=0
WAIT_PID=""

usage() {
  cat <<EOF
Usage: Scripts/uninstall.sh [options]

Remove the per-user ChiaKey installation:
  ${APP}

By default user phrases and settings are kept so a later reinstall picks
them up again.

Options:
  --purge          Also delete user phrases, lexicons, settings, and caches.
  --keep-user-data Keep user data (default).
  --wait-pid PID   Wait for PID to exit before removing files. Used when the
                   preferences app launches this script to uninstall itself.
  --dry-run        Print actions without deleting anything.
  -h, --help       Show this help.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --purge) PURGE=1 ;;
    --keep-user-data) PURGE=0 ;;
    --wait-pid)
      WAIT_PID="${2:-}"
      shift
      ;;
    --dry-run) DRY_RUN=1 ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

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

# Best effort throughout: a partially removed install should still end up
# removed, so individual failures never abort the script.

if [[ -n "${WAIT_PID}" ]]; then
  echo "waiting for pid ${WAIT_PID} to exit"
  for _ in $(seq 1 50); do
    kill -0 "${WAIT_PID}" 2>/dev/null || break
    sleep 0.2
  done
fi

# Take the input source out of the system list while the binary still exists.
if [[ -x "${APP}/Contents/MacOS/${APP_NAME}" ]]; then
  run "${APP}/Contents/MacOS/${APP_NAME}" uninstall || true
fi

# Same process matching as the installer's postinstall, plus anything running
# from inside the bundle (phrase editor, preferences app).
run /usr/bin/pkill -x "${APP_NAME}" || true
run /usr/bin/pkill -x "${LEGACY_APP_NAME}" || true
run /usr/bin/pkill -f "${APP}/Contents/" || true
run /usr/bin/pkill -f "${LEGACY_APP}/Contents/" || true

for bundle in "${APP}" "${LEGACY_APP}"; do
  [[ -e "${bundle}" ]] || continue
  run /bin/rm -rf "${bundle}"
done

for identifier in "${PKG_IDENTIFIERS[@]}"; do
  run /usr/sbin/pkgutil --forget "${identifier}" 2>/dev/null || true
done

if [[ "${PURGE}" == "1" ]]; then
  echo "purging user data and settings"
  [[ -e "${APP_SUPPORT_DIR}" ]] && run /bin/rm -rf "${APP_SUPPORT_DIR}"
  [[ -e "${CACHE_DIR}" ]] && run /bin/rm -rf "${CACHE_DIR}"
  for plist in "${PLIST_GLOB}"*.plist; do
    [[ -e "${plist}" ]] || continue
    run /bin/rm -f "${plist}"
  done
  for domain in "${DEFAULTS_DOMAINS[@]}"; do
    run /usr/bin/defaults delete "${domain}" 2>/dev/null || true
  done
  [[ -e "${UPDATE_LOCK}" ]] && run /bin/rm -f "${UPDATE_LOCK}"
else
  echo "keeping user data in ${APP_SUPPORT_DIR}"
fi

# A system-wide install (CLI-only path) needs root to remove; just point at it.
if [[ -e "/Library/Input Methods/${APP_NAME}.app" ]]; then
  echo "note: system-wide copy found; remove it with:" >&2
  echo "  sudo rm -rf '/Library/Input Methods/${APP_NAME}.app'" >&2
fi

echo "ChiaKey uninstalled. Log out and back in to clear the input source cache."
exit 0
