#!/usr/bin/env bash
# Verify that a ChiaKey installer package carries no PackageKit relocation
# metadata (issue #25).
#
# A component whose PackageInfo lists <relocate> bundle rules lets PackageKit
# redirect the payload onto any existing copy with the same bundle id -- e.g. a
# Downloads backup -- while the install receipt still reports success. The
# top-level relocatable attribute does NOT prevent this: pkgbuild on recent
# macOS emits relocatable="false" together with a non-empty <relocate> list
# unless the component plist forces BundleIsRelocatable=false.
#
# A non-empty <bundle-version> is rejected for the same reason: PackageKit then
# skips the payload when the destination holds a newer bundle, which is the
# other way the payload fails to land where postinstall expects it.
#
# Usage:
#   verify-pkg-no-relocation.sh <pkg>
#       Verify a built package (product or component). Product packages are
#       expanded once and every embedded component is checked.
#   verify-pkg-no-relocation.sh --sanitize <expanded-component-dir>
#       Strip <relocate> rules and the relocatable attribute from the
#       PackageInfo inside an already-expanded component package, then verify
#       it. build-release-package.sh runs this before re-flattening.
set -euo pipefail

fail() {
  echo "verify-pkg-no-relocation: $*" >&2
  exit 1
}

usage() {
  cat >&2 <<EOF
Usage:
  Scripts/verify-pkg-no-relocation.sh <pkg>
      Verify a built package (product or component); embedded components of a
      product package are checked too.
  Scripts/verify-pkg-no-relocation.sh --sanitize <expanded-component-dir>
      Strip <relocate> rules from the PackageInfo of an expanded component
      package, then verify it. Fails on any relocation metadata left behind.
EOF
  exit 2
}

check_package_info() {
  local package_info="$1" label="$2"

  [ -f "${package_info}" ] || fail "${label}: no PackageInfo at ${package_info}"
  if /usr/bin/grep -q '<relocate' "${package_info}"; then
    /usr/bin/grep -n '<relocate' "${package_info}" >&2 || true
    fail "${label}: PackageInfo carries <relocate> rules; PackageKit could redirect the payload onto another copy (issue #25)"
  fi
  if /usr/bin/grep -q 'relocatable="true"' "${package_info}"; then
    fail "${label}: PackageInfo is relocatable; PackageKit could redirect the payload (issue #25)"
  fi
  # A version-checked bundle lets PackageKit keep a newer copy and skip the
  # payload, which the postinstall placement check reports as a failed install.
  if /usr/bin/grep -q '<bundle-version>' "${package_info}"; then
    fail "${label}: PackageInfo version-checks its bundles; PackageKit could skip the payload for a newer copy on disk (issue #25)"
  fi
  echo "OK ${label}: no relocation metadata"
}

# The payload description the installer actually needs. Checked after sanitation
# because the sed below works on lines: a PackageInfo that ever arrives as a
# single line would otherwise be deleted wholesale and still pass the greps.
check_package_info_intact() {
  local package_info="$1" label="$2" element

  /usr/bin/xmllint --noout "${package_info}" >/dev/null 2>&1 ||
    fail "${label}: PackageInfo is no longer well-formed XML"
  for element in '<pkg-info' '<payload' '<bundle id='; do
    /usr/bin/grep -q "${element}" "${package_info}" ||
      fail "${label}: PackageInfo lost ${element}"
  done
}

sanitize_expanded_component() {
  local expanded_dir="$1"
  local package_info="${expanded_dir}/PackageInfo"

  [ -f "${package_info}" ] || fail "--sanitize: no PackageInfo at ${package_info}"
  # Self-closing <relocate/>, block <relocate>...</relocate> (attributes
  # included), and the attribute itself. Anything the patterns miss is caught
  # by the verification below, which fails the build instead of shipping.
  /usr/bin/sed -i '' \
    -e '/<relocate[^>]*\/>/d' \
    -e '/<relocate[ >]/,/<\/relocate>/d' \
    -e 's/relocatable="true"/relocatable="false"/g' \
    "${package_info}"
  check_package_info_intact "${package_info}" "sanitized $(/usr/bin/basename "${expanded_dir}")"
  check_package_info "${package_info}" "sanitized $(/usr/bin/basename "${expanded_dir}")"
}

WORK_DIR=""
cleanup() {
  if [ -n "${WORK_DIR}" ]; then
    /bin/rm -rf "${WORK_DIR}"
  fi
  return 0
}
trap cleanup EXIT

verify_pkg() {
  local pkg="$1"

  [ -f "${pkg}" ] || fail "no such package: ${pkg}"
  WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/chiakey-verify-pkg.XXXXXX")"

  local checked=0
  local nested name
  /usr/sbin/pkgutil --expand "${pkg}" "${WORK_DIR}/pkg"
  if [ -f "${WORK_DIR}/pkg/PackageInfo" ]; then
    check_package_info "${WORK_DIR}/pkg/PackageInfo" "$(basename "${pkg}")"
    checked=1
  fi
  # A product package expands to its embedded components as already-expanded
  # directories; a bare component package has none of these.
  for nested in "${WORK_DIR}/pkg"/*.pkg; do
    [ -e "${nested}" ] || continue
    name="$(/usr/bin/basename "${nested}" .pkg)"
    if [ -f "${nested}/PackageInfo" ]; then
      check_package_info "${nested}/PackageInfo" "${name}"
      checked=1
    elif [ -f "${nested}" ]; then
      /usr/sbin/pkgutil --expand "${nested}" "${WORK_DIR}/${name}"
      check_package_info "${WORK_DIR}/${name}/PackageInfo" "${name}"
      checked=1
    fi
  done
  [ "${checked}" -gt 0 ] || fail "no PackageInfo found inside ${pkg}"
}

case "${1:-}" in
  --sanitize)
    [ $# -eq 2 ] || usage
    sanitize_expanded_component "$2"
    ;;
  -h|--help)
    usage
    ;;
  *)
    [ $# -eq 1 ] || usage
    verify_pkg "$1"
    ;;
esac
