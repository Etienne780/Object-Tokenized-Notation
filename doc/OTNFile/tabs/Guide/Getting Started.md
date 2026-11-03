---
id: node_mu55p95x7rsejz9hraxp
name: Getting Started
---

# Getting Started

OTN (Object Tokenized Notation) is a text format for structured data. Data is stored in **named objects**. Every object works like a table: it has typed columns and any number of rows. Objects can reference rows of other objects, so nested C++ structs can be stored without repeating data.

```otn
@version: 2;

@object: {
	Items[2] {
		int/id, String/name, float/value
	};
	1, "Sword", 42.5;
	2, "Shield", 17.25;
};
```

## Setup

The library consists of two files:

- `OTNFile.h` - the public API
- `OTNFile.cpp` - the implementation

Add both to your project and include the header:

```cpp
#include "OTNFile.h"
```

Requirements:

- C++17 or later
- Tested with MSVC (Windows) and GCC (Linux)

## First example

Build an object, write it to a file and read it back:

{copy}
```cpp
// write
OTN::OTNObject items{ "Items" };
items.SetNames("id", "name", "value")
     .AddDataRow(1, "Sword", 42.5f)
     .AddDataRow(2, "Shield", 17.25f);

OTN::OTNWriter writer;
if (!writer.AppendObject(items).Save("data.otn"))
    std::cerr << writer.GetError() << '\n';

// read
OTN::OTNReader reader;
if (reader.ReadFile("data.otn")) {
    if (auto obj = reader.TryGetObject("Items")) {
        std::string name = obj->GetValue<std::string>(0, "name", "");
        float value = obj->GetValue<float>(0, "value", 0.0f);
    }
}
```

The `.otn` extension is added automatically if a path has no extension.

## Which class to use

| Class | Use it for |
|---|---|
| `OTNObject` | Building and reading the data of one object (table) in memory |
| `OTNWriter` | Writing objects that are completely in memory, see [[writing-data]] |
| `OTNReader` | Reading a whole file into memory, see [[reading-data]] |
| `OTNStreamWriter` | Writing large data row by row, see [[stream-writer]] |
| `OTNStreamReader` | Reading large files row by row or with random access, see [[stream-reader]] |

Your own structs are stored with a small template specialization, see [[custom-types]].

All classes report errors without exceptions, see [[error-handling]].
