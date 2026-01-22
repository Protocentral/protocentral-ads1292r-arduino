#!/bin/bash
#
# Upload Example 04 (Debug Serial) to Arduino Uno R4 Minima
#
# Usage: ./scripts/upload_debug.sh [port]
#   port: Optional serial port (e.g., /dev/cu.usbmodem14101)
#         If not specified, arduino-cli will auto-detect
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIBRARY_DIR="$(dirname "$SCRIPT_DIR")"
EXAMPLE_PATH="$LIBRARY_DIR/examples/04-debug-serial/04-debug-serial.ino"
FQBN="arduino:renesas_uno:minima"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}ADS1292R Debug - Upload to Uno R4${NC}"
echo -e "${GREEN}========================================${NC}"
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

# Determine port
if [ -n "$1" ]; then
    PORT="$1"
    echo -e "Using specified port: ${YELLOW}$PORT${NC}"
else
    echo "Auto-detecting port..."
    PORT=$(arduino-cli board list | grep -i "Arduino UNO R4" | awk '{print $1}' | head -1)
    if [ -z "$PORT" ]; then
        echo -e "${RED}Error: Could not auto-detect Arduino Uno R4 Minima${NC}"
        echo ""
        echo "Available boards:"
        arduino-cli board list
        echo ""
        echo "Usage: $0 [port]"
        exit 1
    fi
    echo -e "Detected port: ${YELLOW}$PORT${NC}"
fi

echo -e "Board: ${YELLOW}$FQBN${NC}"
echo -e "Example: ${YELLOW}$EXAMPLE_PATH${NC}"
echo ""

# Compile
echo -e "${GREEN}Compiling...${NC}"
arduino-cli compile --fqbn "$FQBN" "$EXAMPLE_PATH"

# Upload
echo ""
echo -e "${GREEN}Uploading...${NC}"
arduino-cli upload --fqbn "$FQBN" --port "$PORT" "$EXAMPLE_PATH"

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Upload complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Open Serial Monitor at 115200 baud to see debug output:"
echo "  arduino-cli monitor -p $PORT -c baudrate=115200"
