#!/usr/bin/env bash
# Called only after the complete joint GitHub release is public.
set -euo pipefail
: "${R2_S3_ENDPOINT:?}" "${R2_BUCKET:?}" "${R2_ACCESS_KEY_ID:?}" "${R2_SECRET_ACCESS_KEY:?}"
: "${TAG:?}" "${GITHUB_REPOSITORY:?}" "${RELEASE_DIR:?}"
R2_PUBLIC_BASE_URL="${R2_PUBLIC_BASE_URL:-https://cdn.chiaki.ch}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/chiakey-joint-appcast.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT
r2() {
  local method="$1" key="$2"; shift 2
  curl --silent --show-error --aws-sigv4 "aws:amz:auto:s3" \
    --user "${R2_ACCESS_KEY_ID}:${R2_SECRET_ACCESS_KEY}" \
    -X "$method" "$@" "${R2_S3_ENDPOINT}/${R2_BUCKET}/chiakey/${key}"
}
read_current() {
  local key="$1" destination="$2" status
  status="$(r2 GET "$key" -o "$destination" -w '%{http_code}')"
  case "$status" in
    200) ;;
    404) printf '{"schema":1}\n' > "$destination" ;;
    *) echo "Cannot read $key (HTTP $status); preserving published feeds." >&2; exit 1 ;;
  esac
}
upload_immutable() {
  local key="$1" path="$2" status
  status="$(r2 GET "$key" -o "$WORK_DIR/existing" -w '%{http_code}')"
  case "$status" in
    200) cmp -s "$path" "$WORK_DIR/existing" || { echo "Refusing to overwrite $key" >&2; exit 1; } ;;
    404) r2 PUT "$key" --fail --upload-file "$path" -H 'Cache-Control: public, max-age=31536000, immutable' ;;
    *) echo "Cannot inspect $key (HTTP $status)" >&2; exit 1 ;;
  esac
}
# Preserve GitHub's original publication date across CDN retries.
published_at="$(gh api "repos/$GITHUB_REPOSITORY/releases/tags/$TAG" --jq .published_at)"
[[ -n "$published_at" && "$published_at" != null ]]
version="${TAG#v}"
prerelease=false
[[ "$TAG" == *-beta.* ]] && prerelease=true
pkg="$RELEASE_DIR/ChiaKey-$version.pkg"
exe="$RELEASE_DIR/ChiaKey-Windows-$version-Setup.exe"
[[ -f "$pkg" && -f "$exe" ]]
# Stage and validate BOTH feeds before changing any published pointer.
for platform in macos windows; do
  current_key="updates/$platform/appcast.json"
  if [[ "$platform" == macos ]]; then
    current_key=appcast.json
    artifact="$pkg"
    artifact_key="releases/$(basename "$artifact")"
  else
    artifact="$exe"
    artifact_key="updates/windows/releases/$TAG/$(basename "$artifact")"
  fi
  read_current "$current_key" "$WORK_DIR/current-$platform.json"
  notes_key="updates/$platform/releases/$TAG/release-notes-$platform.md"
  digest="$(shasum -a 256 "$artifact" | awk '{print $1}')"
  jq -n --arg tag "$TAG" --arg version "$version" --arg published_at "$published_at" \
    --arg package_name "$(basename "$artifact")" \
    --arg package_url "$R2_PUBLIC_BASE_URL/chiakey/$artifact_key" \
    --arg notes_url "$R2_PUBLIC_BASE_URL/chiakey/$notes_key" \
    --arg release_url "https://github.com/$GITHUB_REPOSITORY/releases/tag/$TAG" \
    --arg sha256 "$digest" --argjson prerelease "$prerelease" \
    '{tag:$tag,version:$version,published_at:$published_at,package_name:$package_name,
      package_url:$package_url,sha256:$sha256,prerelease:$prerelease,
      notes_url:$notes_url,release_url:$release_url}' > "$WORK_DIR/entry-$platform.json"
  python3 "$ROOT_DIR/Scripts/update-appcast.py" --platform "$platform" \
    --current "$WORK_DIR/current-$platform.json" --entry "$WORK_DIR/entry-$platform.json" \
    --output "$WORK_DIR/feed-$platform.json"
  upload_immutable "$artifact_key" "$artifact"
  upload_immutable "$notes_key" "$RELEASE_DIR/release-notes-$platform.md"
done
for platform in macos windows; do
  r2 PUT "updates/$platform/appcast.json" --fail --upload-file "$WORK_DIR/feed-$platform.json" \
    -H 'Content-Type: application/json' -H 'Cache-Control: public, max-age=300'
done
# The old URL and top-level channel fields stay usable by every shipped Mac client.
r2 PUT appcast.json --fail --upload-file "$WORK_DIR/feed-macos.json" \
  -H 'Content-Type: application/json' -H 'Cache-Control: public, max-age=3600'
if [[ "$prerelease" == false ]]; then
  r2 PUT ChiaKey.pkg --fail --upload-file "$pkg" \
    -H 'Content-Type: application/octet-stream' -H 'Cache-Control: public, max-age=300'
fi
