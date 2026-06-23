#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
IMAGE_DIR="${REPO_ROOT}/YahooKeyKey-Source-1.1.2528/Loaders/OSX-IMK/Images"
SOURCE_SVG="${IMAGE_DIR}/ChiaKey.svg"
OUTPUT_ICNS="${IMAGE_DIR}/ChiaKey.icns"
OUTPUT_ICNS_16="${IMAGE_DIR}/ChiaKey16.icns"
OUTPUT_ICNS_32="${IMAGE_DIR}/ChiaKey32.icns"

require_tool() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "error: missing required tool: $1" >&2
    exit 1
  fi
}

require_tool magick
require_tool ruby

if [[ ! -f "${SOURCE_SVG}" ]]; then
  echo "error: source SVG not found: ${SOURCE_SVG}" >&2
  exit 1
fi

WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/chiakey-icon.XXXXXX")"
trap 'rm -rf "${WORK_DIR}"' EXIT

MASTER_PNG="${WORK_DIR}/ChiaKey.png"
SOURCE_PNG="${WORK_DIR}/ChiaKey-source.png"
SOURCE_ALPHA="${WORK_DIR}/ChiaKey-source-alpha.png"
INK_ALPHA="${WORK_DIR}/ChiaKey-ink-alpha.png"
ICONSET_DIR="${WORK_DIR}/ChiaKey.iconset"
LEGACY_16_RGBA="${WORK_DIR}/icon_16x16.rgba"
LEGACY_32_RGBA="${WORK_DIR}/icon_32x32.rgba"

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

mkdir -p "${ICONSET_DIR}"

for size in 16 32 128 256 512; do
  png_file="${ICONSET_DIR}/icon_${size}x${size}.png"
  magick \
    "${MASTER_PNG}" \
    -filter Lanczos \
    -resize "${size}x${size}" \
    -alpha on \
    -strip \
    "PNG32:${png_file}"
done

for size in 16 32 128 256 512; do
  doubled_size=$((size * 2))
  magick \
    "${MASTER_PNG}" \
    -filter Lanczos \
    -resize "${doubled_size}x${doubled_size}" \
    -alpha on \
    -strip \
    "PNG32:${ICONSET_DIR}/icon_${size}x${size}@2x.png"
done

magick "${ICONSET_DIR}/icon_16x16.png" -depth 8 "rgba:${LEGACY_16_RGBA}"
magick "${ICONSET_DIR}/icon_32x32.png" -depth 8 "rgba:${LEGACY_32_RGBA}"

# Write ICNS directly. Legacy 16/32 chunks use RGB data plus alpha masks;
# Retina and larger sizes use PNG chunks.
ruby - "${ICONSET_DIR}" "${OUTPUT_ICNS}" "${LEGACY_16_RGBA}" "${LEGACY_32_RGBA}" <<'RUBY'
iconset_dir, output_path, legacy_16_rgba, legacy_32_rgba = ARGV
chunks = []

add_chunk = lambda do |type, data|
  chunks << type.b + [data.bytesize + 8].pack("N") + data
end

add_legacy_icon = lambda do |rgb_type, mask_type, rgba_path|
  rgb_data = String.new.b
  mask_data = String.new.b

  File.binread(rgba_path).bytes.each_slice(4) do |red, green, blue, alpha|
    rgb_data << red << green << blue
    mask_data << alpha
  end

  add_chunk.call(rgb_type, rgb_data)
  add_chunk.call(mask_type, mask_data)
end

add_png_icon = lambda do |type, file_name|
  add_chunk.call(type, File.binread(File.join(iconset_dir, file_name)))
end

add_legacy_icon.call("is32", "s8mk", legacy_16_rgba)
add_legacy_icon.call("il32", "l8mk", legacy_32_rgba)
add_png_icon.call("ic11", "icon_16x16@2x.png")
add_png_icon.call("ic12", "icon_32x32@2x.png")
add_png_icon.call("ic07", "icon_128x128.png")
add_png_icon.call("ic13", "icon_128x128@2x.png")
add_png_icon.call("ic08", "icon_256x256.png")
add_png_icon.call("ic14", "icon_256x256@2x.png")
add_png_icon.call("ic09", "icon_512x512.png")
add_png_icon.call("ic10", "icon_512x512@2x.png")

payload = chunks.join

File.binwrite(output_path, "icns".b + [payload.bytesize + 8].pack("N") + payload)
RUBY

# Keep the legacy separate names expected by the project and input-source plist.
cp "${OUTPUT_ICNS}" "${OUTPUT_ICNS_16}"
cp "${OUTPUT_ICNS}" "${OUTPUT_ICNS_32}"

echo "Generated:"
echo "  ${OUTPUT_ICNS}"
echo "  ${OUTPUT_ICNS_16}"
echo "  ${OUTPUT_ICNS_32}"
