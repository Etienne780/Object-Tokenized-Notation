---
id: node_a2e7u4c9h6mpw0sj3kfr
name: Architecture
---

# Architecture

An overview of how the library is built. The following pages describe the parts in detail.

## Layers

```text
 OTNWriter   OTNStreamWriter   OTNReader   OTNStreamReader     classes: flow, objects, references
      \            |               |            /
       \           |               |           /
        Detail::FileHeaderInfo / ObjectHeaderInfo / OTNRow       data passed between the layers
                    |                          |
          Detail::V1 write functions   Detail::V1 read functions   syntax: text <-> info structs
                                               |
                                       Tokenizer / CharSource      tokens from memory or file
```

- The **classes** decide what is written or read and in which order. They resolve and create references and manage files.
- The **syntax layer** is the only code that knows the file syntax, see [[syntax-layer]].
- **OTNObject** and **OTNValue** are the data model that all classes share, see [[otnobject]] and [[type-system]].

## Files

| File | Content |
|---|---|
| `OTNFile/include/OTNFile/OTNFile.h` | Public API, data model, templates (`OTNDataType`, `TryDeserializeValue`), info structs |
| `OTNFile/src/OTNFile/OTNFile.cpp` | Everything else |
| `OTNFileTest/` | Test program, the tests are in `include/Tests/*.h` |

`OTNFile.cpp` is split into regions:

| Region | Content |
|---|---|
| (top) | Helpers: path validation, number parsing, type deduction |
| `Syntax` | `DefTable`, `ParseTypeString`, `CharSource`, `Tokenizer`, `ReadVersion`, `Detail::V1`, `Current`, `GetReadSyntax` |
| `OTNObject` | `OTNObject` and `OTNObjectBuilder` |
| `OTNWriter` | see [[writer-internals]] |
| `OTNReader` | see [[reader-internals]] |
| `OTNStreamWriter` | see [[writer-internals]] |
| `OTNStreamReader` | see [[reader-internals]] |

## Namespaces

- `OTN`: everything a user needs.
- `OTN::Syntax` and `OTN::Keyword`: the characters and keywords of the format.
- `OTN::Detail`: implementation details. Users should not need anything from it.

## Building and testing

The project uses premake (`premake5.lua`, `build.bat`). The test program `OTNFileTest` gets an output folder as argument, runs all tests and prints the number of passed and failed tests:

```text
OTNFileTest <output_directory>
```

The tests are header files per topic in `OTNFileTest/include/Tests/` (`TestObject.h`, `TestWriterReader.h`, `TestStream.h`, `TestBugfixes.h`). The files they write are put into the output folder.
