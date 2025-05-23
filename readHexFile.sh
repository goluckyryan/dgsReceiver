#!/bin/bash

# Check arguments
if [ $# -ne 2 ]; then
    echo "Usage: $0 <binary_file> <num_words>"
    exit 1
fi

binary_file="$1"
num_words="$2"
num_bytes=$((num_words * 4))

# Validate that num_words is a number
if ! [[ "$num_words" =~ ^[0-9]+$ ]]; then
    echo "Error: num_words must be a non-negative integer"
    exit 1
fi

# Run hexdump + awk
hexdump -n "$num_bytes" -v -e '1/4 "%08X\n"' "$binary_file" | awk '{printf "%06d: 0x%s\n", NR-1, $1}'
