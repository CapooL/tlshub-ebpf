#!/bin/bash
#
# TLShub Benchmark Test Suite
# Runs various test scenarios to validate tlshub_bench functionality
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BENCH_BINARY="./tlshub_bench"
SERVER_PORT=19090
TEST_RESULTS_DIR="./test_results"
SERVER_PID=""

# Cleanup function
cleanup() {
    if [ ! -z "$SERVER_PID" ]; then
        echo -e "${YELLOW}Stopping echo server (PID: $SERVER_PID)...${NC}"
        kill $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
    fi
    
    # Clean up test files
    rm -f /tmp/tlshub_bench_server_*.log
}

# Set up cleanup on exit
trap cleanup EXIT INT TERM

# Print header
print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

# Print test result
print_result() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✓ $2${NC}"
    else
        echo -e "${RED}✗ $2${NC}"
    fi
}

# Check if binary exists
if [ ! -f "$BENCH_BINARY" ]; then
    echo -e "${RED}Error: $BENCH_BINARY not found${NC}"
    echo "Please run 'make' first to build the binary"
    exit 1
fi

# Create test results directory
mkdir -p "$TEST_RESULTS_DIR"

print_header "TLShub Benchmark Test Suite"
echo ""

# Test 1: Help message
print_header "Test 1: Help Message"
$BENCH_BINARY --help > /dev/null
print_result $? "Help message displayed successfully"
echo ""

# Test 2: Start echo server
print_header "Test 2: Echo Server Startup"
LOG_FILE="/tmp/tlshub_bench_server_$$.log"
$BENCH_BINARY --mode server --listen-port $SERVER_PORT > "$LOG_FILE" 2>&1 &
SERVER_PID=$!
sleep 2

# Check if server is running
if kill -0 $SERVER_PID 2>/dev/null; then
    print_result 0 "Echo server started successfully (PID: $SERVER_PID)"
else
    print_result 1 "Failed to start echo server"
    cat "$LOG_FILE"
    exit 1
fi
echo ""

# Test 3: Basic client test (small)
print_header "Test 3: Basic Client Test (Small Data)"
$BENCH_BINARY --mode client \
    --target-ip 127.0.0.1 \
    --target-port $SERVER_PORT \
    --data-size 512 \
    --total-connections 5 \
    --json "$TEST_RESULTS_DIR/test3_basic.json" \
    --csv "$TEST_RESULTS_DIR/test3_basic.csv" > /dev/null
print_result $? "Basic client test completed"

# Check if output files were created
if [ -f "$TEST_RESULTS_DIR/test3_basic.json" ] && [ -f "$TEST_RESULTS_DIR/test3_basic.csv" ]; then
    print_result 0 "Output files created (JSON and CSV)"
else
    print_result 1 "Output files not created"
fi
echo ""

# Test 4: Medium data size test
print_header "Test 4: Medium Data Size Test"
$BENCH_BINARY --mode client \
    --target-ip 127.0.0.1 \
    --target-port $SERVER_PORT \
    --data-size 4096 \
    --total-connections 10 \
    --json "$TEST_RESULTS_DIR/test4_medium.json" \
    --csv "$TEST_RESULTS_DIR/test4_medium.csv" > /dev/null
print_result $? "Medium data size test completed"
echo ""

# Test 5: Large data size test
print_header "Test 5: Large Data Size Test"
$BENCH_BINARY --mode client \
    --target-ip 127.0.0.1 \
    --target-port $SERVER_PORT \
    --data-size 65536 \
    --total-connections 5 \
    --json "$TEST_RESULTS_DIR/test5_large.json" \
    --csv "$TEST_RESULTS_DIR/test5_large.csv" > /dev/null
print_result $? "Large data size test completed"
echo ""

# Test 6: Multiple connections test
print_header "Test 6: Multiple Connections Test"
$BENCH_BINARY --mode client \
    --target-ip 127.0.0.1 \
    --target-port $SERVER_PORT \
    --data-size 2048 \
    --total-connections 20 \
    --json "$TEST_RESULTS_DIR/test6_multiple.json" \
    --csv "$TEST_RESULTS_DIR/test6_multiple.csv" > /dev/null
print_result $? "Multiple connections test completed"
echo ""

# Test 7: Validate JSON output
print_header "Test 7: Validate JSON Output"
if command -v jq &> /dev/null; then
    jq empty "$TEST_RESULTS_DIR/test3_basic.json" 2>/dev/null
    print_result $? "JSON output is valid"
else
    echo -e "${YELLOW}jq not installed, skipping JSON validation${NC}"
fi
echo ""

# Test 8: Validate CSV output
print_header "Test 8: Validate CSV Output"
if [ -s "$TEST_RESULTS_DIR/test3_basic.csv" ]; then
    line_count=$(wc -l < "$TEST_RESULTS_DIR/test3_basic.csv")
    if [ $line_count -gt 10 ]; then
        print_result 0 "CSV output has expected content ($line_count lines)"
    else
        print_result 1 "CSV output seems incomplete ($line_count lines)"
    fi
else
    print_result 1 "CSV output is empty or missing"
fi
echo ""

# Summary
print_header "Test Summary"
echo ""
echo "Test results saved to: $TEST_RESULTS_DIR"
echo ""
echo "Files generated:"
ls -lh "$TEST_RESULTS_DIR"
echo ""

# Display sample results
print_header "Sample Results (Test 3)"
if command -v jq &> /dev/null; then
    echo "Summary metrics:"
    jq '.summary_metrics' "$TEST_RESULTS_DIR/test3_basic.json" 2>/dev/null || cat "$TEST_RESULTS_DIR/test3_basic.json"
else
    echo "First 30 lines of CSV:"
    head -30 "$TEST_RESULTS_DIR/test3_basic.csv"
fi
echo ""

print_header "All Tests Completed"
echo -e "${GREEN}✓ Test suite finished successfully${NC}"
echo ""
echo "Review test results in: $TEST_RESULTS_DIR"
echo ""
