#!/bin/bash
# simple_output.sh
#
# This script runs each test case by:
# 1. Removing any existing socket file.
# 2. Starting the engine executable (which listens on a Unix socket) in the background.
# 3. Waiting briefly for the engine to initialize.
# 4. Sending the test case input via netcat (nc) to the Unix socket.
# 5. Allowing extra time for the engine to process input and flush output.
# 6. Killing the engine process.
# 7. Printing the captured output.
#
# Usage: ./simple_output.sh <engine_executable> <socket_path> <test_folder>
# Example: ./simple_output.sh ./engine /tmp/orderbook.sock tests

if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <engine_executable> <socket_path> <test_folder>"
    exit 1
fi

ENGINE="$1"
SOCKET_PATH="$2"
TEST_FOLDER="$3"

for infile in "$TEST_FOLDER"/*.in; do
    testname=$(basename "$infile" .in)
    echo "=== Running test case: $testname ==="
    
    # Remove any existing socket file.
    if [ -e "$SOCKET_PATH" ]; then
        rm "$SOCKET_PATH"
    fi

    # Start the engine in the background, capturing its output (stdout and stderr).
    "$ENGINE" "$SOCKET_PATH" > "$TEST_FOLDER/$testname.actual" 2>&1 &
    engine_pid=$!
    
    # Wait for the engine to initialize.
    sleep 1
    
    # Send test input via netcat to the Unix socket.
    cat "$infile" | nc -U "$SOCKET_PATH"
    
    # Allow extra time for the engine to process and flush output.
    sleep 10
    
    # Kill the engine process.
    kill $engine_pid
    wait $engine_pid 2>/dev/null
    
    # Print the captured output.
    echo "Output for $testname:"
    cat "$TEST_FOLDER/$testname.actual"
    echo "---------------------------"
done
