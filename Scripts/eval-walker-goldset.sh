#!/usr/bin/env bash
set -euo pipefail

# Measures Manjusri walker top-1 accuracy. Not a pass/fail test: it prints
# numbers you compare between two builds or two parameter values, which is why
# it is eval-* rather than test-*.
#
# Usage:
#   Scripts/eval-walker-goldset.sh --corpus FILE [options]
#   Scripts/eval-walker-goldset.sh --gold TSV [options]
#
#   --corpus FILE      plain sentences, one per line; a gold set is built from it
#   --gold TSV         reuse a gold set built earlier (skips the build step)
#   --out TSV          where to write the built gold set (default: temp)
#   --dominance N      0 (default) keeps only characters with a single reading in
#                      the lexicon, so no reading is ever guessed. A positive
#                      value also takes a polyphonic character whose top reading
#                      leads the next by N log10 -- more sentences, but some
#                      readings are inferred and wrong ones cost the walker
#                      accuracy it never had a chance at. Report absolute
#                      numbers from 0 only.
#   --length-prior X   override Node's per-extra-syllable bonus
#   --user-db PATH     attach a user learning database and enable it. Read-only
#                      in the default mode; with --replay it is WRITTEN TO, so
#                      point that at a scratch copy, never at your live
#                      SmartMandarinUserData.db.
#   --mismatches FILE  write every wrong sentence for inspection
#   --limit N          cap the number of gold sentences
#   --replay           drive ManjusriComposer like the IME, correcting mistakes
#                      and letting the real learning path record them; reports
#                      manual selections per pass. Use this for changes to how
#                      strongly learning scores. Without --user-db it learns
#                      into a scratch database under TMPDIR.
#   --passes N         replay the gold set N times (learning carries over)
#   --reset-user-db    with --replay, empty the --user-db first (it is otherwise
#                      added to in place)
#
# Any other flags are passed through to the eval step.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="$ROOT_DIR/ChiaKey-Source"
HEADER_SHIMS="$SOURCE_DIR/Frameworks/HeaderShims"
ACTIVE_DB="${HOME}/Library/Application Support/ChiaKey/Lexicons/active/ChiaKeySource.db"
BUNDLED_DB="$SOURCE_DIR/Distributions/Takao/CookedDatabase/ChiaKeySource.db"
TMP_BASE="$(mktemp -d "${TMPDIR:-/tmp}/chiakey-walker-goldset.XXXXXX")"

trap 'rm -rf "$TMP_BASE"' EXIT

CORPUS=""
GOLD=""
GOLD_OUT="$TMP_BASE/goldset.tsv"
DOMINANCE="0"
LIMIT=""
LEXICON="${CHIAKEY_LOCAL_LEXICON_DB:-}"
EVAL_ARGS=()
BUILD_EXTRA=()
WORD_LEVEL=""
MODE="eval"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --corpus) CORPUS="$2"; shift 2 ;;
    --gold) GOLD="$2"; shift 2 ;;
    --out) GOLD_OUT="$2"; shift 2 ;;
    --dominance) DOMINANCE="$2"; shift 2 ;;
    --limit) LIMIT="$2"; shift 2 ;;
    --replay) MODE="replay"; shift ;;
    --lexicon) LEXICON="$2"; shift 2 ;;
    # word-level build options; they mean nothing to eval, so keep them apart
    --word-level) WORD_LEVEL="1"; shift ;;
    --pins|--variants|--max-variants|--min-chars|--max-chars|--max-word-chars)
      BUILD_EXTRA+=("$1" "$2"); shift 2 ;;
    -h|--help) sed -n '3,28p' "${BASH_SOURCE[0]}"; exit 0 ;;
    *) EVAL_ARGS+=("$1"); shift ;;
  esac
done

if [[ -z "$LEXICON" && -f "$ACTIVE_DB" ]]; then
  LEXICON="$ACTIVE_DB"
fi
if [[ -z "$LEXICON" ]]; then
  LEXICON="$BUNDLED_DB"
fi
if [[ ! -f "$LEXICON" ]]; then
  echo "ChiaKeySource.db not found: $LEXICON" >&2
  exit 1
fi

if [[ -z "$CORPUS" && -z "$GOLD" ]]; then
  echo "one of --corpus or --gold is required" >&2
  exit 1
fi

BIN="$TMP_BASE/walker-goldset"
clang++ \
  -std=c++17 \
  -O2 \
  -DOV_USE_SQLITE \
  -I"$HEADER_SHIMS" \
  -I"$SOURCE_DIR/Frameworks/OpenVanilla/Headers" \
  -I"$SOURCE_DIR/Frameworks/PlainVanilla/Headers" \
  -I"$SOURCE_DIR/Frameworks/Formosa/Headers" \
  -I"$SOURCE_DIR/Frameworks/Manjusri/Headers" \
  -I"$SOURCE_DIR/ModulePackages/OVIMMandarin" \
  "$SOURCE_DIR/Frameworks/Manjusri/Tools/WalkerGoldSet.cpp" \
  "$SOURCE_DIR/Frameworks/Manjusri/Source/Node.cpp" \
  "$SOURCE_DIR/Frameworks/Formosa/Source/Mandarin.cpp" \
  -lsqlite3 \
  -o "$BIN"

if [[ -z "$GOLD" ]]; then
  build_args=(build --lexicon "$LEXICON" --corpus "$CORPUS" --out "$GOLD_OUT")
  if [[ -n "$WORD_LEVEL" ]]; then
    build_args+=(--word-level)
  else
    build_args+=(--dominance "$DOMINANCE")
  fi
  build_args+=(${BUILD_EXTRA[@]+"${BUILD_EXTRA[@]}"})
  if [[ -n "$LIMIT" ]]; then
    build_args+=(--limit "$LIMIT")
  fi
  "$BIN" "${build_args[@]}"
  GOLD="$GOLD_OUT"
  echo "gold set: $GOLD"
fi

"$BIN" "$MODE" --lexicon "$LEXICON" --gold "$GOLD" ${EVAL_ARGS[@]+"${EVAL_ARGS[@]}"}
