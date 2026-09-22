#!/bin/bash
# Turn a prompt file into a short user-facing Chinese summary via the Gemini API.
#
#   Scripts/gemini-summarize.sh <prompt-file> <output-file>
#
# Never fails its caller: on a missing key, an unreachable API or an empty
# answer it writes an empty output file and still exits 0, so a release keeps
# its original notes instead of losing the whole run.

set -uo pipefail

prompt_file="${1:?usage: gemini-summarize.sh <prompt-file> <output-file>}"
out_file="${2:?usage: gemini-summarize.sh <prompt-file> <output-file>}"

# Tried in order: the newest Flash is also the most contended on the free tier,
# so the list ends on an older GA model that is rarely at capacity. Rewriting a
# commit list into a few bullets does not need the newest generation.
models="${GEMINI_MODELS:-gemini-3.8-flash gemini-3.6-flash gemini-2.5-flash}"

: > "${out_file}"

if [[ -z "${GEMINI_API_KEY:-}" ]]; then
  echo "::warning::GEMINI_API_KEY is not set; skipping the Chinese summary."
  exit 0
fi

# --rawfile: the prompt carries commit subjects and release bodies, so it never
# goes through the shell or through string interpolation into JSON.
body="$(jq -n --rawfile p "${prompt_file}" '{contents:[{parts:[{text:$p}]}]}')"
resp="$(mktemp)"
trap 'rm -f "${resp}"' EXIT
code=""

for model in ${models}; do
  for attempt in 1 2 3; do
    code="$(curl -sS --max-time 120 -o "${resp}" -w '%{http_code}' \
      "https://generativelanguage.googleapis.com/v1beta/models/${model}:generateContent" \
      -H "x-goog-api-key: ${GEMINI_API_KEY}" \
      -H 'content-type: application/json' \
      -d "${body}")" || code="000"

    [[ "${code}" == "200" ]] && break

    case "${code}" in
      # "Come back later": retry the same model.
      429|5??|000) ;;
      # 401/403 are the key itself; no model will do better.
      401|403) break 2 ;;
      # Anything else (typically 404 for a model that was renamed or retired,
      # or 400 for one that rejects this request shape) is about *this* model,
      # so stop retrying it but still try the next one.
      *) break ;;
    esac

    echo "${model} returned ${code} (attempt ${attempt}/3)"
    # 30s then 60s: a 503 here is a capacity spike, and the old 15/30 gave up
    # on the whole model list inside two minutes.
    [[ "${attempt}" != "3" ]] && sleep $((attempt * 30))
  done
  [[ "${code}" == "200" ]] && break
  echo "::notice::${model} unavailable (${code}); trying the next model."
done

if [[ "${code}" != "200" ]]; then
  echo "::warning::Gemini request failed (${code}); keeping the original notes."
  head -c 2000 "${resp}"
  exit 0
fi

jq -r '.candidates[0].content.parts[]?.text // empty' "${resp}" > "${out_file}"

if [[ ! -s "${out_file}" ]]; then
  echo "::warning::Gemini returned no text; keeping the original notes."
  head -c 2000 "${resp}"
fi
