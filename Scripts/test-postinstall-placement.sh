#!/usr/bin/env bash
# Regression tests for Packaging/Installer/Scripts/postinstall placement
# behavior (issue #25): the payload must exist -- with the right bundle id and
# version -- at the destination this run installs to, and a missing or stale
# payload must fail the install instead of registering some other copy.
#
# Every run happens inside a sandbox injected into the script's SANDBOX_DIR
# assignment, so nothing here can kill, delete, or register a real ChiaKey
# install on the test machine.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
POSTINSTALL_TEMPLATE="${ROOT_DIR}/Packaging/Installer/Scripts/postinstall"
TEST_DIR="$(mktemp -d "${TMPDIR:-/tmp}/chiakey-postinstall.XXXXXX")"
trap '/bin/rm -rf "${TEST_DIR}"' EXIT

SANDBOX="${TEST_DIR}/world"
EXPECTED_VERSION="1.2.8-test"
BUNDLE_ID="com.chiakey.inputmethod.ChiaKey"
APP_REL="Library/Input Methods/ChiaKey.app"

failures=0

report() { # <ok|not-ok> <name> [detail]
  if [ "$1" = "ok" ]; then
    echo "ok - $2"
  else
    echo "not ok - $2"
    [ $# -gt 2 ] && echo "$3" | /usr/bin/sed 's/^/    /'
    failures=$((failures + 1))
  fi
}

make_app() { # <install-root> <version> <bundle-id>
  local root="$1" version="$2" bundle_id="$3"
  local app="${root}/${APP_REL}"
  mkdir -p "${app}/Contents/MacOS"
  cat >"${app}/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleIdentifier</key><string>${bundle_id}</string>
  <key>CFBundleVersion</key><string>${version}</string>
</dict>
</plist>
EOF
  printf '#!/bin/sh\necho "registration-skipped enable-requested"\n' \
    >"${app}/Contents/MacOS/ChiaKey"
  chmod +x "${app}/Contents/MacOS/ChiaKey"
}

LAST_STATUS=0
LAST_OUTPUT=""

run_postinstall() { # <target-root> <expected-version|unsubstituted>
  local target="$1" expected_version="$2" script="${TEST_DIR}/postinstall"
  local version_expr="s|^EXPECTED_VERSION=.*|EXPECTED_VERSION=\"${expected_version}\"|"
  if [ "${expected_version}" = "unsubstituted" ]; then
    version_expr=""
  fi
  # Same assignment-line-only substitution build-release-package.sh performs.
  /usr/bin/sed \
    -e "s|^SANDBOX_DIR=.*|SANDBOX_DIR=\"${SANDBOX}\"|" \
    ${version_expr:+-e "${version_expr}"} \
    "${POSTINSTALL_TEMPLATE}" >"${script}"
  # Without the sandbox the script would operate on the real "/": refuse to run
  # it at all rather than pkill and delete the tester's own install.
  /usr/bin/grep -qF "SANDBOX_DIR=\"${SANDBOX}\"" "${script}" || {
    echo "failed to inject the sandbox into the postinstall script" >&2
    exit 1
  }
  chmod +x "${script}"
  set +e
  LAST_OUTPUT="$("${script}" "/" "${target}" "${TEST_DIR}/fake.pkg" 2>&1)"
  LAST_STATUS=$?
  set -e
}

expect_status() { # <status> <name>
  if [ "${LAST_STATUS}" -eq "$1" ]; then
    report ok "$2"
  else
    report not-ok "$2" "exit ${LAST_STATUS} (wanted $1); output:
${LAST_OUTPUT}"
  fi
}

expect_output() { # <fixed-string> <name>
  if /usr/bin/grep -qF "$1" <<<"${LAST_OUTPUT}"; then
    report ok "$2"
  else
    report not-ok "$2" "pattern not found: $1; output:
${LAST_OUTPUT}"
  fi
}

expect_no_output() { # <fixed-string> <name>
  if /usr/bin/grep -qF "$1" <<<"${LAST_OUTPUT}"; then
    report not-ok "$2" "unexpected pattern: $1; output:
${LAST_OUTPUT}"
  else
    report ok "$2"
  fi
}

expect_exists() { # <path> <name>
  if [ -e "$1" ]; then
    report ok "$2"
  else
    report not-ok "$2" "missing: $1"
  fi
}

expect_missing() { # <path> <name>
  if [ ! -e "$1" ]; then
    report ok "$2"
  else
    report not-ok "$2" "still present: $1"
  fi
}

reset_world() {
  /bin/rm -rf "${SANDBOX}"
  /bin/mkdir -p "${SANDBOX}/Users/tester"
}

echo "== system-wide install lands at the destination and registers it =="
reset_world
make_app "${SANDBOX}" "${EXPECTED_VERSION}" "${BUNDLE_ID}"
run_postinstall "${SANDBOX}" "${EXPECTED_VERSION}"
expect_status 0 "system-wide install succeeds"
expect_output "registering input source with ${SANDBOX}/${APP_REL}/Contents/MacOS/ChiaKey" "registers the freshly installed system copy"
expect_output "exited with 0" "registration ran"

echo "== relocated payload (nothing at the destination) fails the install =="
reset_world
make_app "${SANDBOX}/Users/tester" "1.2.6" "${BUNDLE_ID}"
run_postinstall "${SANDBOX}" "${EXPECTED_VERSION}"
expect_status 1 "relocated payload fails the install"
expect_no_output "registering input source" "does not register a stale copy"
expect_output "relocated" "diagnoses the likely cause"
expect_exists "${SANDBOX}/Users/tester/${APP_REL}" "leaves the old copy untouched"

echo "== stale version at the destination fails the install =="
reset_world
make_app "${SANDBOX}" "1.2.6" "${BUNDLE_ID}"
run_postinstall "${SANDBOX}" "${EXPECTED_VERSION}"
expect_status 1 "version mismatch fails the install"
expect_no_output "registering input source" "does not register the stale app"

echo "== foreign bundle id at the destination fails the install =="
reset_world
make_app "${SANDBOX}" "${EXPECTED_VERSION}" "com.example.Other"
run_postinstall "${SANDBOX}" "${EXPECTED_VERSION}"
expect_status 1 "bundle id mismatch fails the install"

echo "== per-user install registers the user copy and prunes the system copy =="
reset_world
make_app "${SANDBOX}" "1.0.0" "${BUNDLE_ID}"
make_app "${SANDBOX}/Users/tester" "${EXPECTED_VERSION}" "${BUNDLE_ID}"
run_postinstall "${SANDBOX}/Users/tester" "${EXPECTED_VERSION}"
expect_status 0 "per-user install succeeds"
expect_output "registering input source with ${SANDBOX}/Users/tester/${APP_REL}/Contents/MacOS/ChiaKey" "registers the per-user copy"
expect_missing "${SANDBOX}/${APP_REL}" "prunes the stale system copy"

echo "== system-wide upgrade prunes the per-user and legacy copies =="
reset_world
make_app "${SANDBOX}" "${EXPECTED_VERSION}" "${BUNDLE_ID}"
make_app "${SANDBOX}/Users/tester" "1.2.6" "${BUNDLE_ID}"
mkdir -p "${SANDBOX}/Users/tester/Library/Input Methods/千秋輸入法.app"
run_postinstall "${SANDBOX}" "${EXPECTED_VERSION}"
expect_status 0 "system-wide upgrade succeeds"
expect_exists "${SANDBOX}/${APP_REL}" "keeps the system copy"
expect_missing "${SANDBOX}/Users/tester/${APP_REL}" "prunes the per-user copy"
expect_missing "${SANDBOX}/Users/tester/Library/Input Methods/千秋輸入法.app" "prunes the legacy copy"

echo "== a newer install on disk is reported as such, not as a bad landing =="
reset_world
make_app "${SANDBOX}" "9.9.9" "${BUNDLE_ID}"
run_postinstall "${SANDBOX}" "${EXPECTED_VERSION}"
expect_status 1 "a newer installed copy fails the install"
expect_output "a newer ChiaKey (9.9.9) is already installed" "names the newer copy"
expect_no_output "did not land where this package installed" "does not blame relocation"

echo "== the sandbox hook is not readable from the environment =="
if /usr/bin/grep -q 'CHIAKEY_POSTINSTALL_SANDBOX_DIR' "${POSTINSTALL_TEMPLATE}"; then
  report not-ok "postinstall takes no sandbox path from the environment" \
    "the template still reads CHIAKEY_POSTINSTALL_SANDBOX_DIR"
else
  report ok "postinstall takes no sandbox path from the environment"
fi

echo "== unsubstituted version placeholder skips only the version check =="
reset_world
make_app "${SANDBOX}" "0.0.0-handrun" "${BUNDLE_ID}"
run_postinstall "${SANDBOX}" "unsubstituted"
expect_status 0 "hand-run script without a version still succeeds"

if [ "${failures}" -gt 0 ]; then
  echo "${failures} postinstall placement test(s) failed." >&2
  exit 1
fi
echo "All postinstall placement tests passed."
