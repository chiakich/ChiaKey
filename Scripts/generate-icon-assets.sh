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
SOURCE_PNG="${WORK_DIR}/ChiakiKeyKey-source.png"
SOURCE_ALPHA="${WORK_DIR}/ChiakiKeyKey-source-alpha.png"
INK_ALPHA="${WORK_DIR}/ChiakiKeyKey-ink-alpha.png"
MULTI_TIFF="${WORK_DIR}/ChiakiKeyKey.tiff"
TIFF_FILES=()

magick \
  -background none \
  "${SOURCE_SVG}" \
  -resize 1024x1024 \
  "PNG32:${SOURCE_PNG}"

# The SVG is authored as black ink plus a white cut-out glyph. Convert that
# rendered artwork into a monochrome template icon with transparent cut-outs.
magick "${SOURCE_PNG}" -alpha extract "${SOURCE_ALPHA}"
magick \
  "${SOURCE_PNG}" \
  \( +clone -alpha off -colorspace Gray -negate \) \
  \( "${SOURCE_ALPHA}" \) \
  -delete 0 \
  -compose Multiply \
  -composite \
  "${INK_ALPHA}"
magick \
  -size 1024x1024 xc:black \
  "${INK_ALPHA}" \
  -alpha off \
  -compose CopyOpacity \
  -composite \
  "PNG32:${MASTER_PNG}"

alpha_mean="$(magick "${MASTER_PNG}" -format "%[fx:mean.a]" info:)"
if ! awk "BEGIN { exit (${alpha_mean} > 0.05 && ${alpha_mean} < 0.95 ? 0 : 1) }"; then
  echo "error: rendered SVG produced an unexpected alpha coverage: ${alpha_mean}" >&2
  exit 1
fi

if magick \
  "${MASTER_PNG}" \
  \( \
    +clone \
    -alpha extract \
    -threshold 0 \
  \) \
  -compose CopyOpacity \
  -composite \
  -format "%[fx:mean.a]" info: | awk '{ exit ($1 == 0 ? 0 : 1) }'; then
  echo "error: rendered SVG appears blank" >&2
  exit 1
fi

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
