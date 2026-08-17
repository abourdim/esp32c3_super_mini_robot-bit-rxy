#!/usr/bin/env bash
# layout_cfg.sh — encode/decode the bit-rxy LAYOUT_CFG_BASE64 blob in
# 01_src/03_bit-rxy.cpp
#
# Usage:
#   ./layout_cfg.sh decode [path/to/03_bit-rxy.cpp]
#       Extracts LAYOUT_CFG_BASE64, base64-decodes it, and pretty-prints
#       the resulting JSON to stdout.
#
#   ./layout_cfg.sh encode <path/to/layout.json>
#       Minifies the given JSON file, base64-encodes it, and prints it
#       pre-wrapped into 72-char double-quoted C string literal chunks
#       ready to paste into LAYOUT_CFG_BASE64.
#
# Requires: base64 (coreutils) and python3 (used only as a JSON
# pretty-printer/minifier — no jq dependency).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEFAULT_CPP="$SCRIPT_DIR/01_src/03_bit-rxy.cpp"

usage() {
  echo "Usage:"
  echo "  $0 decode [VAR] [path/to/03_bit-rxy.cpp]"
  echo "      VAR defaults to LAYOUT_CFG_EXPERT_BASE64;"
  echo "      use LAYOUT_CFG_BEGINNER_BASE64 for the simple layout."
  echo "      cpp defaults to $DEFAULT_CPP"
  echo "  $0 encode <path/to/layout.json>"
  exit 1
}

decode() {
  # Two layouts are compiled in now (beginner + expert), so the variable name
  # is an argument. Defaults to the expert one, which is the one being edited
  # most of the time.
  local var="${1:-LAYOUT_CFG_EXPERT_BASE64}"
  local cpp_file="${2:-$DEFAULT_CPP}"
  [ -f "$cpp_file" ] || { echo "error: file not found: $cpp_file" >&2; exit 1; }

  # Pull every quoted string literal between '<var> =' and the terminating
  # ';', strip the quotes, and concatenate them back together.
  local b64
  b64=$(awk -v v="$var" '$0 ~ "^static const char\\* " v {flag=1} flag{print} flag && /;[[:space:]]*$/{exit}' "$cpp_file" \
    | grep -o '"[^"]*"' \
    | sed 's/^"//; s/"$//' \
    | tr -d '\n')

  if [ -z "$b64" ]; then
    echo "error: could not find $var in $cpp_file" >&2
    exit 1
  fi

  echo "$b64" | base64 -d | python3 -m json.tool
}

encode() {
  local json_file="${1:-}"
  [ -n "$json_file" ] || usage
  [ -f "$json_file" ] || { echo "error: file not found: $json_file" >&2; exit 1; }

  local b64
  b64=$(python3 -c "import json,sys; print(json.dumps(json.load(open(sys.argv[1])), separators=(',',':')))" "$json_file" \
    | tr -d '\r\n' \
    | base64 -w0)

  echo "// Paste this in place of the existing LAYOUT_CFG_BASE64 chunks:"
  echo "$b64" | fold -w72 | sed 's/^/  "/; s/$/"/'
  echo ";"
}

case "${1:-}" in
  decode) shift; decode "$@" ;;
  encode) shift; encode "$@" ;;
  *) usage ;;
esac
