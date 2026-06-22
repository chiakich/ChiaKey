#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
IMAGE_DIR="${REPO_ROOT}/YahooKeyKey-Source-1.1.2528/Loaders/OSX-IMK/Images"
SOURCE_SVG="${IMAGE_DIR}/ChiakiKeyKey.svg"
OUTPUT_ICNS="${IMAGE_DIR}/ChiakiKeyKey.icns"
OUTPUT_ICNS_16="${IMAGE_DIR}/ChiakiKeyKey16.icns"
OUTPUT_ICNS_32="${IMAGE_DIR}/ChiakiKeyKey32.icns"

require_tool() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "error: missing required tool: $1" >&2
    exit 1
  fi
}

require_tool magick
require_tool tiff2icns

if [[ ! -f "${SOURCE_SVG}" ]]; then
  echo "error: source SVG not found: ${SOURCE_SVG}" >&2
  exit 1
fi

WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/chiaki-keykey-icon.XXXXXX")"
trap 'rm -rf "${WORK_DIR}"' EXIT

MASTER_PNG="${WORK_DIR}/ChiakiKeyKey.png"
MULTI_TIFF="${WORK_DIR}/ChiakiKeyKey.tiff"
TIFF_FILES=()

# The editable SVG is kept beside the generated icons. Some local ImageMagick
# builds silently rasterize SVG as a transparent blank image, so this script
# mirrors the SVG geometry with ImageMagick's native vector drawing commands.
magick \
  -size 1024x1024 \
  canvas:none \
  -draw "fill none stroke white stroke-width 28 stroke-linejoin round path 'M304,152 H720 C826,152 872,198 872,304 V720 C872,826 826,872 720,872 H304 C198,872 152,826 152,720 V304 C152,198 198,152 304,152 Z' stroke-linecap round stroke-linejoin round stroke-width 72 path 'M344,302 C446,286 562,258 680,222' stroke-width 82 path 'M290,446 H734' stroke-width 86 path 'M520,282 V760'" \
  "PNG32:${MASTER_PNG}"

for size in 16 32 128 256 512; do
  tiff_file="${WORK_DIR}/icon_${size}.tiff"
  magick \
    "${MASTER_PNG}" \
    -filter Lanczos \
    -resize "${size}x${size}" \
    -alpha on \
    -strip \
    "${tiff_file}"
  TIFF_FILES+=("${tiff_file}")
done

magick "${TIFF_FILES[@]}" "${MULTI_TIFF}"
tiff2icns "${MULTI_TIFF}" "${OUTPUT_ICNS}"

# Keep the legacy separate names expected by the project and input-source plist.
cp "${OUTPUT_ICNS}" "${OUTPUT_ICNS_16}"
cp "${OUTPUT_ICNS}" "${OUTPUT_ICNS_32}"

echo "Generated:"
echo "  ${OUTPUT_ICNS}"
echo "  ${OUTPUT_ICNS_16}"
echo "  ${OUTPUT_ICNS_32}"
