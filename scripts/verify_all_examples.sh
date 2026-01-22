#!/bin/bash
#
# Compile and verify all ADS1292R examples for Arduino Uno R4 Minima
#
# Usage: ./scripts/verify_all_examples.sh
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIBRARY_DIR="$(dirname "$SCRIPT_DIR")"
EXAMPLES_DIR="$LIBRARY_DIR/examples"
FQBN="arduino:renesas_uno:minima"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${GREEN}================================================${NC}"
echo -e "${GREEN}ADS1292R Library - Verify All Examples${NC}"
echo -e "${GREEN}Board: Arduino Uno R4 Minima${NC}"
echo -e "${GREEN}================================================${NC}"
echo ""

# Check if arduino-cli is installed
if ! command -v arduino-cli &> /dev/null; then
    echo -e "${RED}Error: arduino-cli is not installed${NC}"
    echo "Install it from: https://arduino.github.io/arduino-cli/"
    exit 1
fi

# Check if the Renesas core is installed
if ! arduino-cli core list | grep -q "arduino:renesas_uno"; then
    echo -e "${YELLOW}Installing Arduino Uno R4 core...${NC}"
    arduino-cli core install arduino:renesas_uno
fi

# Track results
PASSED=0
FAILED=0
FAILED_EXAMPLES=""

# Find all examples
EXAMPLES=$(find "$EXAMPLES_DIR" -name "*.ino" | sort)

if [ -z "$EXAMPLES" ]; then
    echo -e "${RED}Error: No examples found in $EXAMPLES_DIR${NC}"
    exit 1
fi

echo -e "Found examples:"
for example in $EXAMPLES; do
    echo -e "  - $(basename "$(dirname "$example")")"
done
echo ""

# Compile each example
for example in $EXAMPLES; do
    EXAMPLE_NAME=$(basename "$(dirname "$example")")

    echo -e "${BLUE}----------------------------------------${NC}"
    echo -e "${BLUE}Compiling: $EXAMPLE_NAME${NC}"
    echo -e "${BLUE}----------------------------------------${NC}"

    if arduino-cli compile --fqbn "$FQBN" "$example" 2>&1; then
        echo -e "${GREEN}✓ $EXAMPLE_NAME - PASSED${NC}"
        ((PASSED++))
    else
        echo -e "${RED}✗ $EXAMPLE_NAME - FAILED${NC}"
        ((FAILED++))
        FAILED_EXAMPLES="$FAILED_EXAMPLES\n  - $EXAMPLE_NAME"
    fi
    echo ""
done

# Summary
echo -e "${GREEN}================================================${NC}"
echo -e "${GREEN}Summary${NC}"
echo -e "${GREEN}================================================${NC}"
echo -e "Total examples: $((PASSED + FAILED))"
echo -e "${GREEN}Passed: $PASSED${NC}"

if [ $FAILED -gt 0 ]; then
    echo -e "${RED}Failed: $FAILED${NC}"
    echo -e "${RED}Failed examples:$FAILED_EXAMPLES${NC}"
    exit 1
else
    echo -e "${GREEN}All examples compiled successfully!${NC}"
fi
