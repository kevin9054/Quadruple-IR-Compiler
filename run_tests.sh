#!/bin/bash
set -e

# Compile the compiler
g++ -std=c++17 compiler.cpp -o compiler

# Run compiler with provided test input
./compiler input.txt >/dev/null

# Compare generated output with expected answer
if diff -u answer.txt table6.table; then
  echo "Test passed"
else
  echo "Output differs from expected"
  exit 1
fi

