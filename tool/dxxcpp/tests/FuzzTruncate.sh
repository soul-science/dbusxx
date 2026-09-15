#!/bin/sh
# Truncation fuzz: run the tool on every prefix of a .dxx file.
#
# A prefix may be a valid file (exit 0) or a broken one (exit 1, the .dxx error
# path); anything else -- a crash, an assertion, a sanitizer abort, an unexpected
# CMake-level error -- is a finding. This is how the lexer/parser edge cases
# (BOM, numbers, strings, initializer braces) were shaken out by hand.
#
# usage: FuzzTruncate.sh <dxxcpp-binary> <input.dxx>
#
# For a sanitizer run, build with -fsanitize=address,undefined and pass that
# binary plus ASAN_OPTIONS=detect_leaks=0.

set -u

BIN="${1:?usage: FuzzTruncate.sh <dxxcpp-binary> <input.dxx>}"
SRC="${2:?usage: FuzzTruncate.sh <dxxcpp-binary> <input.dxx>}"

if [ ! -f "$BIN" ] || [ ! -f "$SRC" ]; then
    echo "FAIL: '$BIN' or '$SRC' does not exist"
    exit 2
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

total=$(wc -c < "$SRC")
bad=0
i=1
while [ "$i" -le "$total" ]; do
    head -c "$i" "$SRC" > "$WORK/prefix.dxx"
    "$BIN" "$WORK/prefix.dxx" -o "$WORK/out" > "$WORK/log" 2>&1
    code=$?
    if [ "$code" != "0" ] && [ "$code" != "1" ]; then
        echo "FAIL: prefix $i/$total exited with $code"
        head -n 10 "$WORK/log"
        bad=$((bad + 1))
        [ "$bad" -ge 3 ] && break
    fi

    i=$((i + 1))
done

if [ "$bad" != "0" ]; then
    echo "[RESULT] $bad prefix(es) failed out of $total"
    exit 1
fi

echo "[RESULT] $total prefix(es) ok (exit 0/1 only)"
