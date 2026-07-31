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
  echo "  $0 decode [path/to/03_bit-rxy.cpp]   (default: $DEFAULT_CPP)"
  echo "  $0 encode <path/to/layout.json>"
  exit 1
}

decode() {
  local cpp_file="${1:-$DEFAULT_CPP}"
  [ -f "$cpp_file" ] || { echo "error: file not found: $cpp_file" >&2; exit 1; }

  # Pull every quoted string literal between 'LAYOUT_CFG_BASE64 =' and the
  # terminating ';', strip the quotes, and concatenate them back together.
  local b64
  b64=$(awk '/^static const char\* LAYOUT_CFG_BASE64/{flag=1} flag{print} flag && /;[[:space:]]*$/{exit}' "$cpp_file" \
    | grep -o '"[^"]*"' \
    | sed 's/^"//; s/"$//' \
    | tr -d '\n')

  if [ -z "$b64" ]; then
    echo "error: could not find LAYOUT_CFG_BASE64 in $cpp_file" >&2
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
