#!/usr/bin/env bash
# launch.sh — ESP32-C3 robot app helper
# Works on: Git Bash, MSYS2 UCRT, Linux, macOS
#
# Interactive:   ./launch.sh
# Non-inter.:    ./launch.sh <action>      (see ./launch.sh help)
# Port override: PORT=COM7 ./launch.sh upload
# Verbose:       VERBOSE=1 ./launch.sh compile     (no spinner, raw output)
# Fun mode:      FUN=1 ./launch.sh                  (emoji)
# No color:      NO_COLOR=1 ./launch.sh

set -u

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR" || { echo "[ERR] cannot cd to $PROJECT_DIR" >&2; exit 1; }

CHIP="esp32c3"
BAUD="115200"
WEB_PORT="${WEB_PORT:-8000}"
PORT="${PORT:-}"
ASSUME_YES="${ASSUME_YES:-0}"
VERBOSE="${VERBOSE:-0}"
FUN="${FUN:-0}"
LOG_DIR="${LOG_DIR:-$PROJECT_DIR/03_logs}"
LOG_KEEP="${LOG_KEEP:-20}"
NO_LOG="${NO_LOG:-0}"

# Strip --yes/-y/--verbose/--fun from args
_args=()
for _a in "$@"; do
    case "$_a" in
        -y|--yes)     ASSUME_YES=1 ;;
        -v|--verbose) VERBOSE=1 ;;
        --fun)        FUN=1 ;;
        *) _args+=("$_a") ;;
    esac
done
if [ "${#_args[@]}" -gt 0 ]; then set -- "${_args[@]}"; else set --; fi
unset _args _a

# ---------- colors ----------
if [ -t 1 ] && [ -z "${NO_COLOR:-}" ] && [ "${TERM:-}" != "dumb" ]; then
    C_RST=$'\033[0m';  C_BOLD=$'\033[1m';  C_DIM=$'\033[2m'
    C_RED=$'\033[31m'; C_GRN=$'\033[32m';  C_YLW=$'\033[33m'
    C_BLU=$'\033[34m'; C_MAG=$'\033[35m';  C_CYN=$'\033[36m'
else
    C_RST=''; C_BOLD=''; C_DIM=''; C_RED=''; C_GRN=''; C_YLW=''; C_BLU=''; C_MAG=''; C_CYN=''
fi

# fun glyphs (ASCII fallback unless FUN=1)
if [ "$FUN" = "1" ]; then
    G_OK="✓"; G_NO="✗"; G_BOLT="⚡"; G_BOT="🤖"; G_GEAR="⚙"; G_ARROW="→"
else
    G_OK="OK"; G_NO="X"; G_BOLT="*"; G_BOT="[bot]"; G_GEAR="#"; G_ARROW="->"
fi

# ---------- OS detection ----------
detect_os() {
    case "${OSTYPE:-$(uname -s)}" in
        msys*|MINGW*|MSYS*) echo "windows-bash" ;;
        cygwin*)            echo "cygwin" ;;
        linux*|Linux*)      echo "linux" ;;
        darwin*|Darwin*)    echo "macos" ;;
        *)                  echo "unknown" ;;
    esac
}
OS="$(detect_os)"

PIO_WIN="$HOME/.platformio/penv/Scripts/pio.exe"
PIO_NIX="$HOME/.platformio/penv/bin/pio"
ESPTOOL_WIN="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"
ESPTOOL_NIX="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"

# ---------- helpers ----------
have() { command -v "$1" >/dev/null 2>&1; }

# Resolve a working python — on Windows (Git Bash / MSYS2 / Cygwin), prefer native
# Windows Python over MSYS2's /usr/bin/python3 (which often has broken pyexpat,
# breaking pip and PlatformIO).
PYTHON_BIN=""
PYTHON_KIND=""   # windows | msys-mingw | msys-posix | native | unknown

# A python candidate is "real" if `--version` runs in a subshell, in <5s, exit 0.
# Subshell isolates us from hostile aliases/functions that might `exit` or hang.
_python_works() {
    [ -n "$1" ] || return 1
    if have timeout; then
        ( timeout 5 "$1" --version ) >/dev/null 2>&1
    else
        ( "$1" --version ) >/dev/null 2>&1
    fi
}

# Classify by sys.prefix — file extension lies (MSYS2 has python.exe too).
# - msys-posix: /usr/bin/python3 — MSYS2's POSIX runtime, often broken (no pyexpat)
# - msys-mingw: /ucrt64, /mingw64, /clang64 — MSYS2 Windows-native, works fine
# - windows:    python.org installer paths
_classify_python() {
    local prefix
    prefix="$( ( "$1" -c "import sys; print(sys.prefix)" ) 2>/dev/null )"
    case "$prefix" in
        /usr|/usr/*)                                                       echo "msys-posix" ;;
        /ucrt64|/ucrt64/*|/mingw64|/mingw64/*|/mingw32|/mingw32/*)         echo "msys-mingw" ;;
        /clang64|/clang64/*|/clang32|/clang32/*)                           echo "msys-mingw" ;;
        /c/msys*|/c/mingw*|*MSYS*|*MINGW*)                                 echo "msys-posix" ;;
        "") echo "unknown" ;;
        *)  echo "windows" ;;
    esac
}

_try_python() {
    local hint_kind="$1" cand="$2" actual_kind
    [ "${VERBOSE:-0}" = "1" ] && printf '  [resolve_python] trying: %s\n' "$cand" >&2
    have "$cand" || [ -x "$cand" ] || return 1
    _python_works "$cand" || {
        [ "${VERBOSE:-0}" = "1" ] && printf '  [resolve_python] %s did not respond to --version\n' "$cand" >&2
        return 1
    }
    if [ "$OS" = "windows-bash" ] || [ "$OS" = "cygwin" ]; then
        actual_kind="$(_classify_python "$cand")"
        [ "$actual_kind" = "unknown" ] && actual_kind="$hint_kind"
    else
        actual_kind="$hint_kind"
    fi
    PYTHON_BIN="$cand"; PYTHON_KIND="$actual_kind"
    [ "${VERBOSE:-0}" = "1" ] && printf '  [resolve_python] picked: %s [%s]\n' "$cand" "$actual_kind" >&2
    return 0
}

resolve_python() {
    [ -n "$PYTHON_BIN" ] && return 0
    # Acceptable: windows or msys-mingw (real, Windows-native pythons).
    # Defer: msys-posix (MSYS2 /usr/bin/python3 — often broken, last resort).
    local saved_bin="" saved_kind=""
    _accept() {
        case "$PYTHON_KIND" in
            windows|msys-mingw) return 0 ;;
            *)
                saved_bin="$PYTHON_BIN"; saved_kind="$PYTHON_KIND"
                PYTHON_BIN=""; PYTHON_KIND=""
                return 1
                ;;
        esac
    }

    if [ "$OS" = "windows-bash" ] || [ "$OS" = "cygwin" ]; then
        local p user="${USER:-${USERNAME:-}}"
        # Build user-AppData prefixes for both POSIX $HOME and Windows /c/Users/<name>
        local prefixes=()
        prefixes+=("$HOME/AppData/Local/Programs/Python")
        [ -n "$user" ] && prefixes+=("/c/Users/$user/AppData/Local/Programs/Python")
        # 1. python.org standard install paths (per-user installer)
        local pre ver
        for pre in "${prefixes[@]}"; do
            for ver in Python313 Python312 Python311 Python310; do
                _try_python windows "$pre/$ver/python.exe" && _accept && return 0
            done
        done
        # 2. machine-wide installs
        for p in "/c/Python313/python.exe" "/c/Python312/python.exe" "/c/Python311/python.exe" \
                 "/c/Program Files/Python313/python.exe" "/c/Program Files/Python312/python.exe" \
                 "/c/Program Files/Python311/python.exe"; do
            _try_python windows "$p" && _accept && return 0
        done
        # 2. MSYS2 MINGW/UCRT/CLANG pythons (Windows-native, full pyexpat)
        for p in \
            "/ucrt64/bin/python.exe"  "/ucrt64/bin/python3.exe" \
            "/mingw64/bin/python.exe" "/mingw64/bin/python3.exe" \
            "/clang64/bin/python.exe" "/clang64/bin/python3.exe" \
            "/mingw32/bin/python.exe" "/mingw32/bin/python3.exe"; do
            _try_python msys-mingw "$p" && _accept && return 0
        done
        # 3. py launcher / generic .exe on PATH
        _try_python windows "py.exe"      && _accept && return 0
        _try_python windows "python.exe"  && _accept && return 0
        _try_python windows "python3.exe" && _accept && return 0
        # 4. last resort — MSYS2 POSIX python (the broken one)
        _try_python msys-posix "python3" && _accept && return 0
        _try_python msys-posix "python"  && _accept && return 0
        # nothing acceptable — fall back to whatever we saved (msys-posix)
        if [ -n "$saved_bin" ]; then
            PYTHON_BIN="$saved_bin"; PYTHON_KIND="$saved_kind"
            [ "${VERBOSE:-0}" = "1" ] && printf '  [resolve_python] falling back to: %s [%s]\n' "$PYTHON_BIN" "$PYTHON_KIND" >&2
            return 0
        fi
    else
        _try_python native "python3" && return 0
        _try_python native "python"  && return 0
    fi
    return 1
}

py() {
    if ! resolve_python; then
        echo "${C_RED}[ERR]${C_RST} python not found" >&2; return 127
    fi
    "$PYTHON_BIN" "$@"
}

# Returns 0 if a usable (non-broken) python is available.
python_is_usable() {
    resolve_python || return 1
    [ "$PYTHON_KIND" != "msys-posix" ]
}

# Reset cached resolution so resolve_python re-scans after installing python.
forget_python() { PYTHON_BIN=""; PYTHON_KIND=""; }

# Find a working winget binary. MSYS2 doesn't always inherit Windows PATH, so
# we also probe the standard Windows install location.
find_winget() {
    local cand user="${USER:-${USERNAME:-}}"
    for cand in \
        "winget.exe" "winget" \
        "$HOME/AppData/Local/Microsoft/WindowsApps/winget.exe" \
        "/c/Users/$user/AppData/Local/Microsoft/WindowsApps/winget.exe"; do
        [ -n "$cand" ] || continue
        if have "$cand" || [ -x "$cand" ]; then
            if ( timeout 5 "$cand" --version ) >/dev/null 2>&1; then
                echo "$cand"
                return 0
            fi
        fi
    done
    return 1
}

# Auto-install python via winget (Windows). Returns 0 on success.
try_install_python_winget() {
    local wg
    if ! wg="$(find_winget)"; then
        info "  ${C_DIM}(winget not available in this shell — skipping auto-install)${C_RST}"
        return 1
    fi
    info "  installing python automatically via ${C_BOLD}winget${C_RST} (no clicks needed)..."
    info "  ${C_DIM}downloading ~30 MB — please wait${C_RST}"
    with_spinner "winget install Python 3.13" \
        "$wg" install --id Python.Python.3.13 --source winget --silent \
            --accept-source-agreements --accept-package-agreements \
            --scope user
    local rc=$?
    spinner_cleanup
    if [ $rc -eq 0 ]; then
        ok "python installed via winget"
        # winget installs to ~/AppData/Local/Programs/Python — already in our search path.
        # PATH may not be updated for THIS shell, but absolute path will be found.
        forget_python
        if resolve_python && python_is_usable; then
            ok "python detected: $PYTHON_BIN"
            return 0
        else
            hint "winget installed python, but I can't find it on PATH yet"
            info "  ${C_BOLD}open a NEW terminal${C_RST} and re-run: ${C_CYN}./launch.sh${C_RST}"
            return 2
        fi
    fi
    hint "winget install failed (exit $rc) — falling back to manual install"
    return 1
}

# Open the python.org download page and show super-simple instructions.
try_install_python_browser() {
    local url="https://www.python.org/downloads/"
    info "  opening the python download page in your browser..."
    open_url "$url" || true
    cat <<EOF

  ${C_BOLD}what to do on the page that just opened:${C_RST}
    ${C_CYN}1.${C_RST} click the big yellow ${C_BOLD}Download Python 3.x.x${C_RST} button
    ${C_CYN}2.${C_RST} open the file that downloads (a .exe installer)
    ${C_CYN}3.${C_RST} ${C_BOLD}${C_RED}IMPORTANT:${C_RST} tick the box ${C_BOLD}"Add python.exe to PATH"${C_RST} at the bottom
    ${C_CYN}4.${C_RST} click ${C_BOLD}Install Now${C_RST} and wait
    ${C_CYN}5.${C_RST} close this window, open a NEW terminal, run ${C_CYN}./launch.sh${C_RST} again

  ${C_DIM}link if your browser didn't open: $url${C_RST}

EOF
}

print_pyexpat_help() {
    # MSYSTEM tells us which subsystem we're in (UCRT64 / MINGW64 / CLANG64 / MSYS)
    local ms="${MSYSTEM:-}" prefix="ucrt-x86_64"
    case "$ms" in
        UCRT64)  prefix="ucrt-x86_64" ;;
        MINGW64) prefix="x86_64" ;;
        MINGW32) prefix="i686" ;;
        CLANG64) prefix="clang-x86_64" ;;
        CLANG32) prefix="clang-i686" ;;
    esac
    cat <<EOF

${C_YLW}${G_BOT}${C_RST}  your ${C_BOLD}MSYS2 POSIX Python${C_RST} (/usr/bin/python3) is missing ${C_CYN}pyexpat${C_RST}.
   pip and PlatformIO won't run with it.

${C_BOLD}fix A — install the MSYS2 native python${C_RST} ${C_DIM}(recommended, stays in MSYS2)${C_RST}
   shell: ${C_BOLD}${ms:-MSYS2}${C_RST}
   ${C_CYN}pacman -S mingw-w64-${prefix}-python mingw-w64-${prefix}-python-pip${C_RST}
   then re-run me — I'll find it at /${ms,,}/bin/python.exe

${C_BOLD}fix B — install python.org Python${C_RST} ${C_DIM}(works outside MSYS2 too)${C_RST}
   ${C_GRN}${G_ARROW}${C_RST} https://www.python.org/downloads/
   ${C_GRN}${G_OK}${C_RST}   tick ${C_BOLD}"Add python.exe to PATH"${C_RST}
   ${C_GRN}${G_ARROW}${C_RST} open a NEW terminal, then ${C_CYN}./launch.sh${C_RST}

EOF
}

# Convert a Windows path (C:\foo\bar) to MSYS form (/c/foo/bar)
_winpath_to_msys() {
    local p="$1"
    case "$p" in
        [A-Za-z]:*)
            local drive="${p:0:1}"
            drive="$(echo "$drive" | tr 'A-Z' 'a-z')"
            p="/$drive${p:2}"
            p="${p//\\//}"
            ;;
    esac
    echo "$p"
}

# Cache of pio invocation. Either a single binary path, or
# ("$PYTHON_BIN" "-m" "platformio") for module-mode invocation.
PIO_INVOKE=()
PIO_DISPLAY=""

pio_resolve() {
    [ "${#PIO_INVOKE[@]}" -gt 0 ] && return 0
    if have pio;            then PIO_INVOKE=("pio");      PIO_DISPLAY="pio"; return 0; fi
    if [ -x "$PIO_WIN" ];   then PIO_INVOKE=("$PIO_WIN"); PIO_DISPLAY="$PIO_WIN"; return 0; fi
    if [ -x "$PIO_NIX" ];   then PIO_INVOKE=("$PIO_NIX"); PIO_DISPLAY="$PIO_NIX"; return 0; fi
    # Look in python user-site Scripts/bin (where `pip install --user` puts pio.exe)
    if resolve_python 2>/dev/null; then
        local userbase cand
        userbase="$( ( "$PYTHON_BIN" -m site --user-base ) 2>/dev/null )"
        if [ -n "$userbase" ]; then
            userbase="$(_winpath_to_msys "$userbase")"
            for cand in "$userbase/Scripts/pio.exe" "$userbase/bin/pio" "$userbase/Scripts/pio"; do
                if [ -x "$cand" ]; then
                    PIO_INVOKE=("$cand"); PIO_DISPLAY="$cand"; return 0
                fi
            done
        fi
        # Last resort: invoke as python module — works when binary script isn't on PATH
        if ( "$PYTHON_BIN" -m platformio --version ) >/dev/null 2>&1; then
            PIO_INVOKE=("$PYTHON_BIN" "-m" "platformio")
            PIO_DISPLAY="$PYTHON_BIN -m platformio"
            return 0
        fi
    fi
    return 1
}

# pio_path returns a printable representation (used for status display only).
# To invoke pio, callers should use pio_cmd which expands the array safely.
pio_path() { pio_resolve && echo "$PIO_DISPLAY"; }

pio_cmd() {
    pio_resolve || { echo "${C_RED}[ERR]${C_RST} pio not found — run: $0 install-pio" >&2; return 127; }
    "${PIO_INVOKE[@]}" "$@"
}

# Reset the cache (call after install)
forget_pio() { PIO_INVOKE=(); PIO_DISPLAY=""; }

esptool_path() {
    [ -f "$ESPTOOL_NIX" ] && { echo "$ESPTOOL_NIX"; return; }
    [ -f "$ESPTOOL_WIN" ] && { echo "$ESPTOOL_WIN"; return; }
    return 1
}

vscode_cmd() {
    if   have code; then echo "code"
    elif [ "$OS" = "macos" ] && [ -x "/Applications/Visual Studio Code.app/Contents/Resources/app/bin/code" ]; then
        echo "/Applications/Visual Studio Code.app/Contents/Resources/app/bin/code"
    else return 1
    fi
}

detect_port_silent() {
    # 1. user-selected or env override
    if [ -n "$PORT" ]; then echo "$PORT"; return; fi

    # 2. parse pio device list — prefer USB serial over PCI/AMT
    local raw usb_port first_port
    raw="$(pio_cmd device list 2>/dev/null)" || return

    # look for Espressif USB (VID 303A) or generic USB serial first
    usb_port="$(echo "$raw" \
        | grep -B5 -iE 'USB VID:PID=303A|USB VID:PID=1A86|USB VID:PID=10C4|CP210|CH340|CH910|FTDI|USB' \
        | grep -oE '^(COM[0-9]+|/dev/(tty|cu)\.[A-Za-z0-9._-]+|/dev/ttyUSB[0-9]+|/dev/ttyACM[0-9]+)' \
        | head -1)"

    if [ -n "$usb_port" ]; then echo "$usb_port"; return; fi

    # 3. fallback: first port found
    first_port="$(echo "$raw" \
        | grep -oE '^(COM[0-9]+|/dev/(tty|cu)\.[A-Za-z0-9._-]+|/dev/ttyUSB[0-9]+|/dev/ttyACM[0-9]+)' \
        | head -1)"
    echo "$first_port"
}

ask_port() {
    local p; p="$(detect_port_silent)"
    if [ -z "$p" ]; then
        printf '%sport?%s (e.g. COM5, /dev/ttyUSB0): ' "$C_CYN" "$C_RST"
        read -r p
    fi
    echo "$p"
}

# interactive port selector — sets PORT for the session
act_select_port() {
    section "serial ports"
    require_pio || return 1

    local raw ports=() descs=() i=0
    raw="$(pio_cmd device list 2>/dev/null)" || { miss "could not list devices"; return 1; }

    # parse port names
    while IFS= read -r line; do
        local pname
        pname="$(echo "$line" | grep -oE '^(COM[0-9]+|/dev/(tty|cu)\.[A-Za-z0-9._-]+|/dev/ttyUSB[0-9]+|/dev/ttyACM[0-9]+)')"
        if [ -n "$pname" ]; then
            ports+=("$pname")
            # grab description: next line starting with "Description:"
            local desc
            desc="$(echo "$raw" | grep -A3 "^${pname}" | grep -i 'Description:' | sed 's/.*Description: *//')"
            descs+=("${desc:-unknown}")
        fi
    done <<< "$raw"

    if [ ${#ports[@]} -eq 0 ]; then
        miss "no serial ports found — is the board plugged in?"
        return 1
    fi

    # detect which one we'd auto-pick
    local auto; auto="$(detect_port_silent)"

    printf '\n'
    for i in "${!ports[@]}"; do
        local marker=""
        if [ "${ports[$i]}" = "$auto" ]; then marker=" ${C_GRN}${G_ARROW} auto-detected${C_RST}"; fi
        if [ "${ports[$i]}" = "$PORT" ]; then marker=" ${C_CYN}${G_ARROW} selected${C_RST}"; fi
        printf '  %s%d)%s  %s%-8s%s  %s%s%s\n' \
            "$C_BOLD$C_CYN" "$((i+1))" "$C_RST" \
            "$C_BOLD" "${ports[$i]}" "$C_RST" \
            "$C_DIM" "${descs[$i]}" "$C_RST$marker"
    done
    printf '  %s0)%s  %sauto-detect%s\n' "$C_DIM" "$C_RST" "$C_DIM" "$C_RST"
    printf '\n'

    printf '%sselect port [1-%d, 0=auto]:%s ' "$C_CYN" "${#ports[@]}" "$C_RST"
    read -r choice

    case "$choice" in
        0|"")
            PORT=""
            local a; a="$(detect_port_silent)"
            ok "auto-detect → ${C_BOLD}${a:-none}${C_RST}"
            ;;
        *)
            if [ "$choice" -ge 1 ] 2>/dev/null && [ "$choice" -le "${#ports[@]}" ] 2>/dev/null; then
                PORT="${ports[$((choice-1))]}"
                ok "port set → ${C_BOLD}$PORT${C_RST}  (for this session)"
            else
                miss "invalid choice: $choice"
            fi
            ;;
    esac
}

pause() { printf '\n%spress enter to continue...%s ' "$C_DIM" "$C_RST"; read -r _; }

confirm() {
    local q="$1" def="${2:-Y}" ans
    if [ "$ASSUME_YES" = "1" ]; then return 0; fi
    if [ "$def" = "Y" ]; then printf '%s [Y/n] ' "$q"; else printf '%s [y/N] ' "$q"; fi
    read -r ans
    ans="${ans:-$def}"
    case "$ans" in [Yy]*) return 0 ;; *) return 1 ;; esac
}

# ---------- spinner / status ----------
TIPS=(
    "hold BOOT + tap RESET if upload fails"
    "ESP32-C3 USB-CDC works without an external UART"
    "0xC0FFEE is a perfectly valid magic number"
    "if it works once, it's a feature; twice, it's documented"
    "always ground yourself before touching the GPIOs"
    "// segfaults are just spicy features"
    "rubber duck > stack overflow (sometimes)"
    "RTFM — but only after you've tried it"
    "115200 baud or it didn't happen"
    "the bug is always in the last place you forgot to look"
    "real hackers solder at 350°C"
    "if in doubt, blink an LED"
    "smoke is the universal hardware error code"
)
QUOTES=(
    "// it works on my board"
    "// have you tried turning the BOOT pin off and on again?"
    "// 'temporary' fix, est. 2017"
    "// here be dragons"
    "// FIXME: write a real fix"
    "// I have no idea why this works"
    "// do not touch — kernel panic on Tuesdays"
)

random_tip()   { echo "${TIPS[$RANDOM % ${#TIPS[@]}]}"; }
random_quote() { echo "${QUOTES[$RANDOM % ${#QUOTES[@]}]}"; }

# spinner: usage:  with_spinner "label" command args...
# After call, $LAST_SPINNER_LOG holds the captured output path (caller may grep it).
# Caller is responsible for cleanup via spinner_cleanup, otherwise next with_spinner
# call will overwrite it.
# ---------- logging ----------
# Slugify a label into a filename-safe token
_slug() { echo "$1" | tr 'A-Z ' 'a-z_' | tr -cd 'a-z0-9_-' | head -c 30; }

# Allocate a fresh timestamped log path: logs/YYYY-MM-DD_HHMMSS_<slug>.log
_new_log_path() {
    [ "$NO_LOG" = "1" ] && return 1
    mkdir -p "$LOG_DIR" 2>/dev/null || return 1
    local ts; ts="$(date +%Y-%m-%d_%H%M%S)"
    echo "$LOG_DIR/${ts}_$(_slug "$1").log"
}

# Pretty-print a command line (quote args containing spaces)
_qcmd() {
    local out="" a
    for a in "$@"; do
        case "$a" in
            *' '*|*'"'*|*\'*) out="$out '$a'" ;;
            *)                out="$out $a" ;;
        esac
    done
    echo "${out# }"
}

# Convert "python -m platformio ..." → "pio ..."  (and same for esptool).
# Returns empty string if no shortening applies.
_short_cmd() {
    if [ "$#" -ge 3 ] && [ "$2" = "-m" ]; then
        case "$3" in
            platformio) shift 3; _qcmd pio "$@"; return ;;
            esptool)    shift 3; _qcmd esptool "$@"; return ;;
        esac
    fi
    echo ""
}

# Print "$ <full>" then optionally "hint: <short>" if a shorter form exists.
# Special-case: "pio_cmd" (our wrapper function) is expanded to the resolved
# invocation so the user sees the real command, not the function name.
_print_cmd_lines() {
    local args=()
    if [ "${1:-}" = "pio_cmd" ] && [ "${#PIO_INVOKE[@]}" -gt 0 ]; then
        shift
        args=("${PIO_INVOKE[@]}" "$@")
    else
        args=("$@")
    fi
    printf '   %s$ %s%s\n' "$C_DIM" "$(_qcmd "${args[@]}")" "$C_RST"
    local short; short="$(_short_cmd "${args[@]}")"
    if [ -n "$short" ]; then
        printf '   %shint:%s %s\n' "$C_YLW" "$C_RST" "$short"
    fi
}

# Trim old logs to LOG_KEEP entries
_rotate_logs() {
    [ "$NO_LOG" = "1" ] && return 0
    [ -d "$LOG_DIR" ] || return 0
    local count; count=$(ls -1t "$LOG_DIR"/*.log 2>/dev/null | wc -l)
    if [ "$count" -gt "$LOG_KEEP" ]; then
        ls -1t "$LOG_DIR"/*.log 2>/dev/null | tail -n +"$((LOG_KEEP+1))" | xargs -r rm -f
    fi
}

# Print "log: <path>" footer
_log_footer() {
    [ -n "$1" ] && [ -f "$1" ] && \
        printf '   %slog:%s %s\n' "$C_DIM" "$C_RST" "$1"
}

# with_timer: stream output live + print elapsed + save to log. Use for compile/upload.
with_timer() {
    local label="$1"; shift
    local logf; logf="$(_new_log_path "$label" 2>/dev/null || true)"
    printf '%s%s%s %s...\n' "$C_CYN" "$G_GEAR" "$C_RST" "$label"
    _print_cmd_lines "$@"
    local start now elapsed rc
    start=$(date +%s)
    if [ -n "$logf" ]; then
        "$@" 2>&1 | tee "$logf"
        rc=${PIPESTATUS[0]}
    else
        "$@"
        rc=$?
    fi
    now=$(date +%s); elapsed=$((now - start))
    if [ "$rc" -eq 0 ]; then
        printf '%s%s%s %s done in %ds\n' "$C_GRN" "$G_OK" "$C_RST" "$label" "$elapsed"
    else
        printf '%s%s%s %s failed (exit %d) in %ds\n' "$C_RED" "$G_NO" "$C_RST" "$label" "$rc" "$elapsed"
    fi
    _log_footer "$logf"
    _rotate_logs
    return "$rc"
}

LAST_SPINNER_LOG=""
spinner_cleanup() {
    # Only delete temp logs — persistent logs in $LOG_DIR are kept for review.
    case "$LAST_SPINNER_LOG" in
        /tmp/*) rm -f "$LAST_SPINNER_LOG" ;;
    esac
    LAST_SPINNER_LOG=""
}

with_spinner() {
    local label="$1"; shift
    spinner_cleanup
    LAST_SPINNER_LOG="$(_new_log_path "$label" 2>/dev/null || mktemp 2>/dev/null || echo "/tmp/launch_$$.log")"
    printf '%s%s%s %s...\n' "$C_CYN" "$G_GEAR" "$C_RST" "$label"
    _print_cmd_lines "$@"
    if [ "$VERBOSE" = "1" ] || [ ! -t 1 ]; then
        "$@" 2>&1 | tee "$LAST_SPINNER_LOG"
        local rc=${PIPESTATUS[0]}
        _log_footer "$LAST_SPINNER_LOG"
        _rotate_logs
        return "$rc"
    fi
    local frames='|/-' i=0 pid start now elapsed rc
    frames="${frames}\\"
    start=$(date +%s)
    "$@" >"$LAST_SPINNER_LOG" 2>&1 &
    pid=$!
    printf '\033[?25l'   # hide cursor
    while kill -0 "$pid" 2>/dev/null; do
        now=$(date +%s); elapsed=$((now - start))
        printf '\r%s%s%s %s ... %02d:%02d ' \
            "$C_CYN" "${frames:$((i%4)):1}" "$C_RST" \
            "$label" $((elapsed/60)) $((elapsed%60))
        i=$((i+1))
        sleep 0.1
    done
    wait "$pid"; rc=$?
    now=$(date +%s); elapsed=$((now - start))
    printf '\033[?25h\r'
    if [ $rc -eq 0 ]; then
        printf '%s%s%s %s done in %ds\n' "$C_GRN" "$G_OK" "$C_RST" "$label" "$elapsed"
    else
        printf '%s%s%s %s failed (exit %d) — last lines:\n' "$C_RED" "$G_NO" "$C_RST" "$label" "$rc"
        if [ $rc -eq 127 ]; then
            printf '  %s(exit 127 = command not found — first arg "%s" is not a real executable)%s\n' \
                "$C_YLW" "${1:-?}" "$C_RST"
        fi
        if [ -s "$LAST_SPINNER_LOG" ]; then
            tail -n 20 "$LAST_SPINNER_LOG" | sed 's/^/  /'
        else
            printf '  %s(no output captured)%s\n' "$C_DIM" "$C_RST"
        fi
    fi
    _log_footer "$LAST_SPINNER_LOG"
    _rotate_logs
    return $rc
}

# ---------- decoration ----------
ROBOTS=(
"   [O.O]    "
"   <(^_^)>  "
"   [-_-]    "
"   {*o*}    "
"   |o_o|    "
)

random_robot() { echo "${ROBOTS[$RANDOM % ${#ROBOTS[@]}]}"; }

BAR_WIDTH=62
# pad_box "raw text without ANSI" "colored content" -> prints "| colored ...  |" padded to BAR_WIDTH
pad_box() {
    local raw="$1" colored="$2"
    local len=${#raw}
    local pad=$((BAR_WIDTH - len - 2))
    [ "$pad" -lt 0 ] && pad=0
    printf '%s|%s %s%*s%s |%s\n' "$C_MAG" "$C_RST" "$colored" "$pad" "" "$C_MAG" "$C_RST"
}

banner() {
    local pio_status port
    if pio_path >/dev/null; then pio_status="${C_GRN}${G_OK}${C_RST}"
    else                          pio_status="${C_RED}${G_NO}${C_RST}"; fi
    port="$(detect_port_silent)"
    [ -z "$port" ] && port="${C_DIM}none${C_RST}"

    local bar; bar=$(printf '=%.0s' $(seq 1 $((BAR_WIDTH - 2))))
    local l1_raw="${G_BOLT} ROBOT-01 LAUNCHER  .  ESP32-C3 firmware toolkit"
    local l1="${C_BOLD}${C_CYN}${G_BOLT} ROBOT-01 LAUNCHER${C_RST}  ${C_DIM}.${C_RST}  ${C_YLW}ESP32-C3${C_RST} firmware toolkit"
    local l2_raw="workshop-diy.org  .  hack stuff  .  break it twice"
    local l2="${C_DIM}workshop-diy.org  .  hack stuff  .  break it twice${C_RST}"
    local l3="${C_DIM}os:${C_RST} ${C_BLU}$OS${C_RST}   ${C_DIM}port:${C_RST} ${port}   ${C_DIM}pio:${C_RST} ${pio_status}"
    local q; q="$(random_quote)"
    local q_col="${C_DIM}${q}${C_RST}"

    printf '%s+%s+%s\n' "$C_MAG" "$bar" "$C_RST"
    pad_box "$l1_raw" "$l1"
    pad_box "$l2_raw" "$l2"
    printf '%s+%s+%s\n' "$C_MAG" "$bar" "$C_RST"
    printf ' %s  ' "$(random_robot)"
    printf '%s\n' "$l3"
    printf '              %s\n' "$q_col"
}

footer() {
    cat <<EOF
${C_DIM}---------------------------------------------------------------${C_RST}
 ${C_YLW}tip:${C_RST} $(random_tip)
 ${C_DIM}(c) workshop-diy.org  .  solder responsibly${C_RST}
${C_DIM}---------------------------------------------------------------${C_RST}
EOF
}

section() { printf '\n%s== %s ==%s\n' "$C_BOLD$C_CYN" "$1" "$C_RST"; }

# auto-heal: ensure pio is installed before running pio-dependent actions
require_pio() {
    if pio_path >/dev/null; then return 0; fi
    printf '\n%s%s%s pio is missing — needed for this action.\n' "$C_YLW" "$G_BOLT" "$C_RST"
    if confirm "  install it now? (option 0 / bootstrap)"; then
        ASSUME_YES=1 act_bootstrap
        pio_path >/dev/null
    else
        miss "skipped — try option 0 (setup my robot) when ready"
        return 1
    fi
}

require_esptool() {
    if esptool_path >/dev/null; then return 0; fi
    printf '\n%s%s%s esptool is missing — needed for erase.\n' "$C_YLW" "$G_BOLT" "$C_RST"
    if confirm "  install esp32 platform now?"; then
        require_pio || return 1
        ASSUME_YES=1 act_install_deps
        esptool_path >/dev/null
    else
        return 1
    fi
}
ok()      { printf '  %s[%s]%s   %s\n' "$C_GRN" "$G_OK"   "$C_RST" "$1"; }
miss()    { printf '  %s[MISS]%s %s\n' "$C_RED" "$C_RST" "$1"; }
hint()    { printf '  %s[HINT]%s %s\n' "$C_YLW" "$C_RST" "$1"; }
info()    { printf '  %s\n' "$1"; }

# ---------- actions ----------
act_check() {
    section "environment check"
    info "${C_DIM}os:${C_RST}      $OS"
    info "${C_DIM}project:${C_RST} $PROJECT_DIR"
    if resolve_python; then
        local pyver; pyver="$("$PYTHON_BIN" --version 2>&1)"
        ok "python: $PYTHON_BIN ($pyver) [$PYTHON_KIND]"
        if [ "$PYTHON_KIND" = "msys-posix" ] && [ "$OS" = "windows-bash" ]; then
            hint "MSYS2 POSIX python — likely breaks pip/pio (missing pyexpat)"
            info "    ${C_DIM}install MSYS2 native python or python.org Python — see option 0${C_RST}"
        fi
    else
        miss "python   ${G_ARROW} install Python 3.x"
    fi
    if p=$(pio_path); then ok "pio: $p ($($p --version 2>&1 | head -1))"
    else                   miss "pio      ${G_ARROW} run: $0 install-pio"
    fi
    if e=$(esptool_path); then ok "esptool: $e"
    else                       miss "esptool  ${G_ARROW} run: $0 install-deps"
    fi
    if v=$(vscode_cmd); then ok "vscode:  $v"
    else                     miss "vscode   ${G_ARROW} https://code.visualstudio.com/"
    fi
    local p; p="$(detect_port_silent)"
    info "${C_DIM}port:${C_RST}    ${p:-${C_DIM}none detected${C_RST}}"
    if [ "$OS" = "linux" ] && ! id -nG "$USER" 2>/dev/null | grep -qw dialout; then
        hint "linux: sudo usermod -aG dialout \$USER  (then re-login)"
    fi
}

act_install_pio() {
    section "installing platformio"
    if ! python_is_usable; then
        # Try to fix python first (transparent winget on Windows)
        if [ "$OS" = "windows-bash" ] || [ "$OS" = "cygwin" ]; then
            if ! try_install_python_winget; then
                try_install_python_browser
                return 1
            fi
        else
            miss "python missing — install first:"
            case "$OS" in
                linux) info "  sudo apt install python3 python3-pip" ;;
                macos) info "  brew install python" ;;
            esac
            return 1
        fi
    fi

    with_spinner "pip install platformio" \
        "$PYTHON_BIN" -m pip install --user -U platformio
    local rc=$?
    if [ "$rc" -ne 0 ]; then
        if [ -n "$LAST_SPINNER_LOG" ] && \
           grep -qE 'pyexpat|No such process|xmlrpc.*expat|from pyexpat' "$LAST_SPINNER_LOG"; then
            print_pyexpat_help
        fi
        spinner_cleanup
        return 1
    fi
    spinner_cleanup

    forget_pio
    if pio_resolve; then
        ok "pio detected: $PIO_DISPLAY"
        local userbase scripts_dir
        userbase="$( ( "$PYTHON_BIN" -m site --user-base ) 2>/dev/null )"
        if [ -n "$userbase" ]; then
            userbase="$(_winpath_to_msys "$userbase")"
            case "$OS" in
                windows-bash|cygwin) scripts_dir="$userbase/Scripts" ;;
                *)                   scripts_dir="$userbase/bin" ;;
            esac
            if [ -d "$scripts_dir" ]; then
                echo
                info "${C_DIM}to call 'pio' directly from any shell, add this to ~/.bashrc:${C_RST}"
                info "  ${C_CYN}export PATH=\"$scripts_dir:\$PATH\"${C_RST}"
                info "${C_DIM}(not required — this script invokes pio via 'python -m platformio')${C_RST}"
            fi
        fi
    else
        miss "pio installed but I still can't find it"
        info "  try opening a NEW terminal and running ${C_CYN}./launch.sh${C_RST} again"
        return 1
    fi
}

act_install_deps() {
    section "installing platform + libs"
    require_pio || return 1
    with_spinner "pio pkg install" pio_cmd pkg install
}

act_bootstrap() {
    # Always run hands-free — kids shouldn't see prompts
    local SAVED_YES="$ASSUME_YES"; ASSUME_YES=1

    section "${G_BOT} setting up your robot — sit back, this takes a few minutes"
    echo

    # Step 1: ensure a USABLE python — auto-install if missing or broken
    info "${C_BOLD}step 1/4${C_RST}  checking python..."
    if ! python_is_usable; then
        if [ -n "$PYTHON_BIN" ] && [ "$PYTHON_KIND" = "msys-posix" ]; then
            info "  found MSYS2 python — but it's missing pyexpat (won't work)"
        else
            info "  no python found"
        fi
        # Try winget first (transparent, no clicks)
        if [ "$OS" = "windows-bash" ] || [ "$OS" = "cygwin" ]; then
            if try_install_python_winget; then
                : # success — PYTHON_BIN now set
            else
                # Fallback: open browser + instructions
                try_install_python_browser
                ASSUME_YES="$SAVED_YES"
                return 1
            fi
        else
            miss "python not found"
            case "$OS" in
                linux) info "  ${C_CYN}sudo apt install python3 python3-pip${C_RST}  (or your distro's equivalent)" ;;
                macos) info "  ${C_CYN}brew install python${C_RST}   ${C_DIM}(or download from python.org)${C_RST}" ;;
            esac
            info "  then re-run me: ${C_CYN}./launch.sh${C_RST}"
            ASSUME_YES="$SAVED_YES"
            return 1
        fi
    fi
    ok "python is ready ($PYTHON_BIN) [$PYTHON_KIND]"

    # Step 2: platformio
    echo
    info "${C_BOLD}step 2/4${C_RST}  checking robot toolkit (platformio)..."
    if pio_path >/dev/null; then
        ok "robot toolkit already installed"
    else
        info "  installing — this is a one-time download (~30 MB)"
        act_install_pio || { ASSUME_YES="$SAVED_YES"; return 1; }
        forget_pio
        if ! pio_resolve; then
            miss "install finished but pio still not findable"
            info "  please open a NEW terminal and re-run: ${C_CYN}./launch.sh${C_RST}"
            ASSUME_YES="$SAVED_YES"
            return 1
        fi
    fi

    # Step 3: esp32 platform + project libs
    echo
    info "${C_BOLD}step 3/4${C_RST}  checking robot board files (esp32 + libraries)..."
    if esptool_path >/dev/null; then
        ok "board files already installed"
    else
        info "  downloading — this is the longest step (~150 MB, grab a snack)"
        with_spinner "pio pkg install" pio_cmd pkg install || { ASSUME_YES="$SAVED_YES"; return 1; }
    fi

    # Step 4: vscode extension (optional, no failure)
    echo
    info "${C_BOLD}step 4/4${C_RST}  checking code editor (vscode, optional)..."
    if c=$(vscode_cmd); then
        if "$c" --list-extensions 2>/dev/null | grep -qi 'platformio.platformio-ide'; then
            ok "vscode + platformio extension ready"
        else
            info "  installing platformio extension for vscode..."
            "$c" --install-extension platformio.platformio-ide >/dev/null 2>&1 \
                && ok "vscode extension installed" \
                || hint "couldn't install vscode extension — skip for now"
        fi
    else
        hint "vscode not installed (optional) — get it at https://code.visualstudio.com/"
    fi

    # Linux serial group hint
    if [ "$OS" = "linux" ] && ! id -nG "$USER" 2>/dev/null | grep -qw dialout; then
        echo
        hint "${C_YLW}one more thing for linux:${C_RST}"
        info "  to talk to the robot over USB, run this once and log out/in:"
        info "  ${C_CYN}sudo usermod -aG dialout \$USER${C_RST}"
    fi

    echo
    printf '%s================================================================%s\n' "$C_GRN" "$C_RST"
    printf '%s  %s  YOUR ROBOT IS READY!  %s%s\n' "$C_GRN$C_BOLD" "$G_BOT" "$G_BOLT" "$C_RST"
    printf '%s================================================================%s\n' "$C_GRN" "$C_RST"
    echo
    info "${C_BOLD}what to do next:${C_RST}"
    info "  ${C_CYN}1)${C_RST} plug your robot into USB"
    info "  ${C_CYN}2)${C_RST} pick option ${C_BOLD}10${C_RST} (full flash) to send the program to the robot"
    info "  ${C_CYN}3)${C_RST} watch your robot wake up ${G_BOT}"
    echo
    info "${C_DIM}stuck? pick option 1 to see what's installed.${C_RST}"

    ASSUME_YES="$SAVED_YES"
}

act_compile() { section "compile"; require_pio || return 1; with_timer "pio run" pio_cmd run; }
act_clean()   { section "clean"; require_pio || return 1; with_timer "pio run -t clean" pio_cmd run -t clean; }

act_upload() {
    require_pio || return 1
    local port; port="$(ask_port)"; [ -z "$port" ] && { miss "no port"; return 1; }
    section "upload ${G_ARROW} $port"
    with_timer "pio upload" pio_cmd run -t upload --upload-port "$port"
}

act_erase() {
    require_esptool || return 1
    local port; port="$(ask_port)"; [ -z "$port" ] && { miss "no port"; return 1; }
    section "erase flash ${G_ARROW} $port"
    local e; e=$(esptool_path)
    with_spinner "esptool erase_flash" py "$e" --chip "$CHIP" --port "$port" erase_flash
}

act_monitor() {
    require_pio || return 1
    local port; port="$(ask_port)"; [ -z "$port" ] && { miss "no port"; return 1; }
    section "serial monitor ${G_ARROW} $port @ $BAUD"
    info "${C_DIM}ctrl+c to quit${C_RST}"
    pio_cmd device monitor -b "$BAUD" --port "$port"
}

act_ports() { section "serial devices"; require_pio || return 1; pio_cmd device list; }
act_flash() { act_compile && act_upload && act_monitor; }

act_code() {
    section "open in vscode"
    local c; c=$(vscode_cmd) || { miss "'code' not on PATH — install vscode and enable shell command"; return 1; }
    if ! "$c" --list-extensions 2>/dev/null | grep -qi 'platformio.platformio-ide'; then
        if confirm "platformio ide extension not installed — install now?"; then
            "$c" --install-extension platformio.platformio-ide
        fi
    fi
    "$c" "$PROJECT_DIR"
    ok "vscode launched"
}

act_home() {
    section "platformio home (browser dashboard)"
    info "${C_DIM}ctrl+c to stop${C_RST}"
    pio_cmd home
}

open_url() {
    local url="$1"
    case "$OS" in
        macos)        have open && open "$url" & ;;
        linux)        have xdg-open && xdg-open "$url" >/dev/null 2>&1 & ;;
        windows-bash) have start && start "" "$url" || powershell.exe -Command "Start-Process '$url'" ;;
    esac
}

act_serve() {
    [ -d "$PROJECT_DIR/02_web" ] || { miss "no 02_web/ folder"; return 1; }
    section "serving 02_web/ on http://localhost:$WEB_PORT"
    info "  ${G_ARROW} flasher: ${C_CYN}http://localhost:$WEB_PORT/flash.html${C_RST}  (chrome/edge only)"
    info "  ${G_ARROW} monitor: ${C_CYN}http://localhost:$WEB_PORT/monitor.html${C_RST}"
    info "  ${C_DIM}ctrl+c to stop${C_RST}"
    open_url "http://localhost:$WEB_PORT/" || true
    ( cd "$PROJECT_DIR/02_web" && py -m http.server "$WEB_PORT" )
}

act_package_web() {
    section "packaging firmware for web flasher"
    with_timer "pio run" pio_cmd run || return 1
    local build_dir="$PROJECT_DIR/.pio/build/esp32-c3-devkitm-1"
    local out="$PROJECT_DIR/02_web/firmware"
    mkdir -p "$out"
    local missing=0
    for f in bootloader.bin partitions.bin firmware.bin; do
        if [ -f "$build_dir/$f" ]; then
            cp "$build_dir/$f" "$out/$f"
            ok "copied $f"
        else
            miss "missing $build_dir/$f"; missing=1
        fi
    done
    [ "$missing" -eq 0 ] && info "next: ${C_CYN}$0 serve${C_RST}"
}

act_update() {
    section "update libraries + platform"
    require_pio || return 1
    with_spinner "pio pkg update" pio_cmd pkg update
    local rc=$?
    spinner_cleanup
    if [ "$rc" -ne 0 ]; then
        miss "pio pkg update failed (exit $rc) — see log above"
        return "$rc"
    fi
    echo
    if confirm "also upgrade PlatformIO Core itself (may break things)?" "N"; then
        with_spinner "pio upgrade" pio_cmd upgrade
        spinner_cleanup
    fi
    ok "update done"
}

act_logs() {
    section "logs"
    if [ ! -d "$LOG_DIR" ]; then
        info "no logs yet — they appear after the first compile/install/upload"
        info "${C_DIM}log dir: $LOG_DIR${C_RST}"
        return 0
    fi
    local files; files=$(ls -1t "$LOG_DIR"/*.log 2>/dev/null)
    if [ -z "$files" ]; then
        info "log dir is empty"
        info "${C_DIM}$LOG_DIR${C_RST}"
        return 0
    fi
    info "${C_DIM}log dir: $LOG_DIR${C_RST}"
    echo
    local i=1 line
    while IFS= read -r line; do
        printf '  %s%2d)%s %s  %s(%s)%s\n' \
            "$C_BOLD" "$i" "$C_RST" "$(basename "$line")" \
            "$C_DIM" "$(wc -l <"$line" | tr -d ' ') lines" "$C_RST"
        i=$((i+1))
        [ "$i" -gt 20 ] && break
    done <<< "$files"
    echo
    printf '%schoice (number to view, q to skip):%s ' "$C_BOLD$C_CYN" "$C_RST"
    local pick; read -r pick
    case "$pick" in
        q|Q|"") return 0 ;;
        *[!0-9]*) miss "not a number"; return 1 ;;
    esac
    local target; target=$(echo "$files" | sed -n "${pick}p")
    [ -z "$target" ] && { miss "no such log"; return 1; }
    if have less; then less -R "$target"
    else cat "$target"
    fi
}

act_git_status() {
    section "git status"
    git -C "$PROJECT_DIR" status
}

act_git_log() {
    section "git log"
    git -C "$PROJECT_DIR" log --oneline --graph --decorate -20
}

act_git_diff() {
    section "git diff"
    git -C "$PROJECT_DIR" diff --stat
    echo
    git -C "$PROJECT_DIR" diff
}

act_help() {
    cat <<EOF
${C_BOLD}usage:${C_RST} $0 [action] [flags]

${C_CYN}actions:${C_RST}
  (none)          interactive menu
  bootstrap       check + install everything missing  ${C_DIM}(alias: setup)${C_RST}
  check           verify python / pio / esptool / vscode / port
  install-pio     pip install platformio
  install-deps    pio pkg install (esp32 + libs)
  compile|build   pio run
  clean           pio run -t clean
  upload          pio run -t upload  ${C_DIM}(auto-detects port)${C_RST}
  erase           esptool erase_flash
  monitor         serial monitor @ $BAUD
  ports           list serial devices
  flash           compile + upload + monitor
  code|vscode     open project in vscode
  home            platformio home (web dashboard)
  serve           serve web/ on :$WEB_PORT  ${C_DIM}(web flasher + monitor)${C_RST}
  package-web     build + copy firmware bins to web/firmware/
  logs            list & view past command logs
  update          update libraries / esp32 platform (pio pkg update)
  help            this message

${C_CYN}flags:${C_RST}
  -y, --yes       skip prompts in bootstrap
  -v, --verbose   raw command output (no spinner)
      --fun       enable emoji glyphs

${C_CYN}env:${C_RST}
  PORT=COM7       override serial port
  WEB_PORT=8080   override web server port
  LOG_DIR=path    where to write logs (default: ./logs)
  LOG_KEEP=20     how many log files to keep (older are auto-deleted)
  NO_LOG=1        disable logging
  NO_COLOR=1      disable ANSI colors

${C_DIM}$(random_quote)${C_RST}
EOF
}

# ---------- menu ----------
MENU_EXPAND=""   # which section is expanded: s, b, w, g, or ""

_menu_section() {
    # $1=key $2=color $3=icon $4=label $5=hint $6=items (multiline)
    local key="$1" col="$2" icon="$3" label="$4" hint="$5" items="$6"
    if [ "$MENU_EXPAND" = "$key" ]; then
        printf '  %s%s)%s %s▾ %s %s%s\n' "$C_BOLD$col" "$key" "$C_RST" "$col" "$icon" "$label" "$C_RST"
        printf '%s\n' "$items"
    else
        printf '  %s%s)%s %s▸ %s %s%s  %s%s%s\n' \
            "$C_BOLD$col" "$key" "$C_RST" \
            "$col" "$icon" "$label" "$C_RST" \
            "$C_DIM" "$hint" "$C_RST"
    fi
}

menu() {
    clear 2>/dev/null || true
    banner

    printf '\n'
    printf '  %s>>>  0) %s SETUP MY ROBOT  (one-click install — start here!)  <<<%s\n\n' \
        "$C_BOLD$C_GRN" "$G_BOT" "$C_RST"

    _menu_section s "$C_YLW" "⚙️ " "toolchain" "— check & install the goods" "$(cat <<EOF
    ${C_YLW}1)${C_RST} check what's installed
    ${C_YLW}2)${C_RST} install platformio only
    ${C_YLW}3)${C_RST} install board files only
EOF
)"

    _menu_section b "$C_RED" "🔥" "deploy" "— burn firmware into silicon" "$(cat <<EOF
    ${C_RED}4)${C_RST}  compile            ${C_DIM}# pio run${C_RST}
    ${C_RED}5)${C_RST}  clean              ${C_DIM}# pio run -t clean${C_RST}
    ${C_RED}6)${C_RST}  upload (flash)     ${C_DIM}# pio run -t upload${C_RST}
    ${C_RED}7)${C_RST}  erase flash        ${C_DIM}# esptool erase_flash${C_RST}
    ${C_RED}8)${C_RST}  serial monitor     ${C_DIM}# pio device monitor${C_RST}
    ${C_RED}9)${C_RST}  select port        ${C_DIM}# pick COM / ttyUSB${C_RST}
   ${C_RED}10)${C_RST}  full flash         ${C_DIM}# compile ${G_ARROW} upload ${G_ARROW} monitor${C_RST}
EOF
)"

    _menu_section w "$C_CYN" "🌐" "cyberspace" "— IDE, dashboards & web tools" "$(cat <<EOF
   ${C_CYN}11)${C_RST}  open in vscode
   ${C_CYN}12)${C_RST}  platformio home    ${C_DIM}# web dashboard${C_RST}
   ${C_CYN}13)${C_RST}  serve 02_web/      ${C_DIM}# flasher + monitor on :$WEB_PORT${C_RST}
   ${C_CYN}14)${C_RST}  package firmware   ${C_DIM}# for web flasher${C_RST}
   ${C_CYN}15)${C_RST}  view logs          ${C_DIM}# past command logs${C_RST}
   ${C_CYN}16)${C_RST}  update libraries   ${C_DIM}# pio + esp32 + lib_deps${C_RST}
EOF
)"

    _menu_section g "$C_MAG" "📡" "git ops" "— track your code" "$(cat <<EOF
   ${C_MAG}17)${C_RST}  git status         ${C_DIM}# what changed?${C_RST}
   ${C_MAG}18)${C_RST}  git log            ${C_DIM}# commit history${C_RST}
   ${C_MAG}19)${C_RST}  git diff           ${C_DIM}# line-by-line changes${C_RST}
EOF
)"

    printf '\n  %s⚡ shortcuts:%s  4=build  10=flash  8=monitor  q=quit\n' "$C_DIM" "$C_RST"

    footer
    printf '%schoice:%s ' "$C_BOLD$C_CYN" "$C_RST"
    read -r choice
    case "$choice" in
        # section toggles — expand/collapse, then redraw
        s|S) if [ "$MENU_EXPAND" = "s" ]; then MENU_EXPAND=""; else MENU_EXPAND="s"; fi; return ;;
        b|B) if [ "$MENU_EXPAND" = "b" ]; then MENU_EXPAND=""; else MENU_EXPAND="b"; fi; return ;;
        w|W) if [ "$MENU_EXPAND" = "w" ]; then MENU_EXPAND=""; else MENU_EXPAND="w"; fi; return ;;
        g|G) if [ "$MENU_EXPAND" = "g" ]; then MENU_EXPAND=""; else MENU_EXPAND="g"; fi; return ;;
        # direct actions — number works even when collapsed
        0)  act_bootstrap ;;
        1)  act_check ;;
        2)  act_install_pio ;;
        3)  act_install_deps ;;
        4)  act_compile ;;
        5)  act_clean ;;
        6)  act_upload ;;
        7)  act_erase ;;
        8)  act_monitor ;;
        9)  act_select_port ;;
        10) act_flash ;;
        11) act_code ;;
        12) act_home ;;
        13) act_serve ;;
        14) act_package_web ;;
        15) act_logs ;;
        16) act_update ;;
        17) act_git_status ;;
        18) act_git_log ;;
        19) act_git_diff ;;
        q|Q) printf '\n%s%s%s bye — may your bytes flow free\n\n' "$C_MAG" "$G_BOT" "$C_RST"; exit 0 ;;
        *)  printf '\n%s%s%s unknown: %s\n' "$C_RED" "$G_NO" "$C_RST" "$choice" ;;
    esac
    pause
}

# ---------- entry ----------
if [ "$#" -gt 0 ]; then
    case "$1" in
        bootstrap|setup) act_bootstrap ;;
        check)        act_check ;;
        install-pio)  act_install_pio ;;
        install-deps) act_install_deps ;;
        compile|build)act_compile ;;
        clean)        act_clean ;;
        upload)       act_upload ;;
        erase)        act_erase ;;
        monitor)      act_monitor ;;
        ports)        act_select_port ;;
        flash)        act_flash ;;
        code|vscode)  act_code ;;
        home)         act_home ;;
        serve)        act_serve ;;
        package-web)  act_package_web ;;
        logs)         act_logs ;;
        update)       act_update ;;
        git-status)   act_git_status ;;
        git-log)      act_git_log ;;
        git-diff)     act_git_diff ;;
        help|-h|--help) act_help ;;
        *) printf '%s%s%s unknown action: %s\n\n' "$C_RED" "$G_NO" "$C_RST" "$1"; act_help; exit 2 ;;
    esac
    exit $?
fi

while true; do menu; done
