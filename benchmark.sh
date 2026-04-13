#!/bin/bash
# Usage: ./benchmark.sh [--csv] [--N] (N = millions of records)
# Examples:
#   ./benchmark.sh           # run all, table output
#   ./benchmark.sh --csv     # run all, save CSV to runs/
#   ./benchmark.sh --10      # run 10M records
#   ./benchmark.sh --csv --5 # run 5M, save CSV

cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="" 2>&1 | grep -v "^\-\-"
cmake --build build --parallel

./build/OrderBook "$@"
