# Francis Compiler (C++)

This project provides a minimal Francis language cross compiler implemented in C++.
It demonstrates the typical phases of a compiler:

* **Lexer** – tokenizes a user-specified source file while populating tables for
  delimiters, reserved words, integers, reals and identifiers.
* **Parser** – performs simple recursive-descent style parsing for declarations,
  assignments, `IF/GTO` statements, array accesses and labels.
* **Semantic Analyzer** – records variable and array type information.
* **IR Builder** – generates a quadruple-based intermediate representation.

After compilation only a single file is written:

* `table6.table` – the quadruple-based intermediate representation

## Building

```bash
g++ -std=c++17 compiler.cpp -o compiler
```

## Running

```bash
./compiler
```

When run, the compiler prompts for a source file name (e.g. `input.txt`) and
writes the intermediate code to `table6.table` in the repository root.
