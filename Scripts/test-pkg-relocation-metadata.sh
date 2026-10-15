#!/usr/bin/env bash
# Regression test for the installer package relocation metadata (issue #25).
#
# A component package built from pkgbuild's defaults carries <relocate> bundle
# rules that let PackageKit redirect the payload onto any existing copy with
# the same bundle id -- even though the top-level relocatable attribute reads
# "false". This test proves the default output fails
# Scripts/verify-pkg-no-relocation.sh, and that the recipe
# Scripts/build-release-package.sh uses (analyze, force the bundle entries
# non-relocatable and not version-checked, sanitize the expanded component)
# passes it.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERIFIER="${ROOT_DIR}/Scripts/verify-pkg-no-relocation.sh"
TEST_DIR="$(mktemp -d "${TMPDIR:-/tmp}/chiakey-pkg-relocation.XXXXXX")"
trap '/bin/rm -rf "${TEST_DIR}"' EXIT

BUNDLE_ID="com.chiakey.inputmethod.ChiaKey"
STAGE_ROOT="${TEST_DIR}/root"

make_stage() {
  local app="${STAGE_ROOT}/Library/Input Methods/ChiaKey.app"
  mkdir -p "${app}/Contents/MacOS"
  cat >"${app}/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleIdentifier</key><string>${BUNDLE_ID}</string>
  <key>CFBundleVersion</key><string>1.2.8</string>
</dict>
</plist>
EOF
  printf '#!/bin/sh\n' >"${app}/Contents/MacOS/ChiaKey"
  chmod +x "${app}/Contents/MacOS/ChiaKey"
}

build_component() { # <out.pkg> [component-plist]
  local out="$1" plist="${2:-}"
  local args=(--root "${STAGE_ROOT}" --identifier test.chiakey.component
    --version 1.2.8 --install-location "/")
  if [ -n "${plist}" ]; then
    args+=(--component-plist "${plist}")
  fi
  /usr/bin/pkgbuild "${args[@]}" "${out}" >/dev/null
}

make_stage

# 1. What an unpatched build produces: the verifier must reject it.
build_component "${TEST_DIR}/default.pkg"
if "${VERIFIER}" "${TEST_DIR}/default.pkg" >/dev/null 2>&1; then
  echo "not ok - default pkgbuild component must fail the relocation verifier" >&2
  exit 1
fi
echo "ok - default pkgbuild component carries relocation metadata (verifier rejects it)"

# 2. The fixed recipe, mirroring build-release-package.sh: analyze, force every
# bundle entry non-relocatable, rebuild, sanitize the expanded component.
/usr/bin/pkgbuild --analyze --root "${STAGE_ROOT}" "${TEST_DIR}/component.plist" >/dev/null
"${ROOT_DIR}/Scripts/force-component-nonrelocatable.py" "${TEST_DIR}/component.plist"
build_component "${TEST_DIR}/forced.pkg" "${TEST_DIR}/component.plist"

/usr/sbin/pkgutil --expand "${TEST_DIR}/forced.pkg" "${TEST_DIR}/expanded"
"${VERIFIER}" --sanitize "${TEST_DIR}/expanded" >/dev/null
/usr/sbin/pkgutil --flatten "${TEST_DIR}/expanded" "${TEST_DIR}/clean.pkg"
"${VERIFIER}" "${TEST_DIR}/clean.pkg" >/dev/null
echo "ok - forced + sanitized component passes the relocation verifier"

# 3. A line-based sanitizer must not quietly empty a PackageInfo that arrives
# as a single line: the stripped file has to fail, not pass.
/usr/sbin/pkgutil --expand "${TEST_DIR}/default.pkg" "${TEST_DIR}/expanded-default"
/bin/mkdir -p "${TEST_DIR}/one-line"
/usr/bin/tr -d '\n' <"${TEST_DIR}/expanded-default/PackageInfo" \
  >"${TEST_DIR}/one-line/PackageInfo"
if "${VERIFIER}" --sanitize "${TEST_DIR}/one-line" >/dev/null 2>&1; then
  echo "not ok - sanitizing a single-line PackageInfo must fail, not pass" >&2
  exit 1
fi
echo "ok - a single-line PackageInfo cannot be silently emptied"

# 4. The release gate runs on the product package, so cover the wrapping too.
for pkg_name in default clean; do
  /bin/cat >"${TEST_DIR}/Distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <choices-outline><line choice="default"/></choices-outline>
  <choice id="default"><pkg-ref id="test.chiakey.component"/></choice>
  <pkg-ref id="test.chiakey.component" version="1.2.8">${pkg_name}.pkg</pkg-ref>
</installer-gui-script>
EOF
  /usr/bin/productbuild --distribution "${TEST_DIR}/Distribution.xml" \
    --package-path "${TEST_DIR}" "${TEST_DIR}/product-${pkg_name}.pkg" >/dev/null
done
if "${VERIFIER}" "${TEST_DIR}/product-default.pkg" >/dev/null 2>&1; then
  echo "not ok - product pkg with relocation metadata must fail the verifier" >&2
  exit 1
fi
echo "ok - product pkg with relocation metadata is rejected"
"${VERIFIER}" "${TEST_DIR}/product-clean.pkg" >/dev/null
echo "ok - product pkg built from the sanitized component passes the verifier"

echo "All pkg relocation metadata tests passed."
