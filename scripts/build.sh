#!/usr/bin/env bash
# Compile every example for the CI board list with arduino-cli, against this
# checkout (the repo does not need to be in the sketchbook).
#
#   scripts/build.sh                 # all boards
#   scripts/build.sh arduino:avr:uno # one board
set -euo pipefail

LIB_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BOARDS=("$@")
if [ ${#BOARDS[@]} -eq 0 ]; then
    BOARDS=(arduino:avr:uno arduino:avr:nano arduino:avr:mega arduino:avr:leonardo
            arduino:renesas_uno:unor4wifi arduino:mbed_nano:nano33ble
            esp32:esp32:esp32 esp32:esp32:esp32c3 rp2040:rp2040:rpipico)
fi

fail=0
for fqbn in "${BOARDS[@]}"; do
    for sketch in "$LIB_DIR"/examples/*/; do
        name="$(basename "$sketch")"
        if out=$(arduino-cli compile -b "$fqbn" --library "$LIB_DIR" --warnings all "$sketch" 2>&1); then
            ram=$(echo "$out" | grep -o 'Global variables use [0-9]* bytes' | grep -o '[0-9]*' || true)
            printf '  ok    %-30s %-22s %s B RAM\n' "$fqbn" "$name" "${ram:-?}"
        else
            printf '  FAIL  %-30s %s\n' "$fqbn" "$name"
            echo "$out" | grep -E 'error' | head -5
            fail=1
        fi
    done
done
exit $fail
