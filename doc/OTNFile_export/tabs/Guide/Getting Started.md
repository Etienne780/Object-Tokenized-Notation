---
id: node_mu55p95x7rsejz9hraxp
name: Getting Started
---

# Getting Started

OTN (Object Tokenized Notation) is a text-based serialization format that stores structured data in a tabular style — similar to a spreadsheet or database table. Data is organized in **named objects**, each with typed columns and any number of data rows.

## What you need

Only two files are required:

- `OTNFile.h` — the full API header
- `OTNFile.cpp` — the implementation

Add both to your project and include the header:

```cpp
#include "OTNFile.h"
```

## How it works

Everything is built around `OTNObject`, which is like a table: it has a name, column names, column types, and rows of data. You build one or more objects in memory, write them to a `.otn` file with `OTNWriter`, and read them back with `OTNReader`.

## Requirements

- C++17 or later
- Tested on Windows

---

> **Tip:** The `.otn` extension is added automatically if you omit it from any file path.
