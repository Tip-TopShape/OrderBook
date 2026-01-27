#!/bin/bash
#
# Run Order Book with Live Dashboard
#
# Usage:
#   ./dashboard/run_with_dashboard.sh
#   ./dashboard/run_with_dashboard.sh --build  # Rebuild C++ first
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/cmake-build-debug"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Order Book Metrics Dashboard${NC}"
echo "=============================="

# Check for --build flag
if [[ "$1" == "--build" ]]; then
    echo -e "${YELLOW}Building C++ application...${NC}"
    cd "$PROJECT_DIR"
    cmake -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
    cmake --build cmake-build-debug
    echo ""
fi

# Check if executable exists
if [[ ! -f "$BUILD_DIR/OrderBook_v2" ]]; then
    echo "Error: OrderBook_v2 executable not found at $BUILD_DIR/OrderBook_v2"
    echo "Run with --build flag to build first: $0 --build"
    exit 1
fi

# Check Python dependencies
echo -e "${YELLOW}Checking Python dependencies...${NC}"
pip3 install -q -r "$SCRIPT_DIR/requirements.txt"

# Create named pipe for communication
PIPE="/tmp/orderbook_metrics_$$"
mkfifo "$PIPE" 2>/dev/null || true

cleanup() {
    echo -e "\n${YELLOW}Shutting down...${NC}"
    rm -f "$PIPE"
    kill $CPP_PID 2>/dev/null
    kill $PY_PID 2>/dev/null
    exit 0
}
trap cleanup SIGINT SIGTERM

echo -e "${GREEN}Starting dashboard...${NC}"
echo ""

# Run C++ app and pipe to Python dashboard
cd "$PROJECT_DIR"
"$BUILD_DIR/OrderBook_v2" 2>&1 | python3 "$SCRIPT_DIR/metrics_dashboard.py" &
PY_PID=$!

wait $PY_PID
cleanup
