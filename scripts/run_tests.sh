#!/bin/bash
set -e

# Compile the compiler
g++ -std=c++17 compiler.cpp -o compiler

# Run compiler with first test input
printf "input.txt\n" | ./compiler >/dev/null
if ! diff -u answer.txt table6.table; then
  echo "Output differs from expected for input.txt"
  exit 1
fi

# Run compiler with second test input
printf "input2.txt\n" | ./compiler >/dev/null
if ! diff -u answer2.txt table6.table; then
  echo "Output differs from expected for input2.txt"
  exit 1
fi

# Run compiler with third test input
printf "input3.txt\n" | ./compiler >/dev/null
if ! diff -u output3.txt table6.table; then
  echo "Output differs from expected for input3.txt"
  exit 1
fi

echo "Test passed"

