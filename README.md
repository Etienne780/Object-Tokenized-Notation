# OTNFile — Object Tokenized Notation

A lightweight C++17 library for structured data serialization using a custom `.otn` text format.

## Features

- **Tabular data model** — data is organized in named objects with typed columns and rows, similar to a dataframe or a database table
- **Type-safe** — supports `int`, `int64`, `uint64`, `float`, `double`, `bool`, `String`, `any`, nested objects, and multi-dimensional arrays
- **Custom type support** — extend serialization to your own types via template specialization
- **Two writers:**
  - `OTNWriter` — collects objects in memory and writes them at once, with optional size optimizations
  - `OTNStreamWriter` — writes objects row by row, for data that does not fit into memory
- **Two readers:**
  - `OTNReader` — loads the entire file into memory and resolves all object references
  - `OTNStreamReader` — keeps only a small index in memory, reads rows sequentially or by object name and row index
- **Checked writing** — names and values are checked before they are written, a written file can always be read again
- **Fluent API** — method chaining for building objects and writing files
- **Single-file integration** — only `OTNFile.h` and `OTNFile.cpp` are required

## Requirements

- C++17 or later
- Windows (tested); other platforms may work but are untested

## Installation

Copy `OTNFile.h` and `OTNFile.cpp` into your project and include the header:

```cpp
#include "OTNFile.h"
```

## Quick Start

### Writing

```cpp
OTN::OTNObject obj("Items");
obj.SetNames("id", "name", "value")
   .AddDataRow(1, "Sword", 42.5f)
   .AddDataRow(2, "Shield", 17.3f);

OTN::OTNWriter writer;
if (!writer.AppendObject(obj).Save("data.otn"))
    std::cerr << writer.GetError() << "\n";
```

`SaveToString()` returns the text instead of writing a file.

### Reading

```cpp
OTN::OTNReader reader;
if (reader.ReadFile("data.otn")) {
    const OTN::OTNObject* obj = reader.FindObject("Items"); // or TryGetObject() for a copy
    if (obj) {
        int id = obj->GetValue<int>(0, "id");
        std::string name = obj->GetValue<std::string>(0, "name");

        // With fallback default
        float value = obj->GetValue<float>(0, "value", 0.0f);

        // As optional
        std::optional<int> maybeId = obj->TryGetValue<int>(0, "id");
    }
}
```

`ReadString()` reads OTN text from a string.

### Streaming large files

Writing row by row:

```cpp
OTN::OTNStreamWriter w;
w.Open("enemies.otn");
w.BeginObject("Enemy",
    OTN::col("name", "String"),
    OTN::col("hp",   "int"),
    OTN::col("pos",  "Vector2"));   // custom type, see below

for (const Enemy& e : enemies)
    w.WriteRow(e.name, e.hp, e.pos);

w.EndObject();
if (!w.Close())
    std::cerr << w.GetError() << "\n";
```

Rows are collected in a temporary file (`enemies.otn.tmp`), the target file is written on `Close()`.

Reading row by row or directly:

```cpp
OTN::OTNStreamReader r;
if (r.Open("enemies.otn")) {
    while (r.NextObject()) {
        std::cout << r.GetCurrentObjectName() << "\n";
        OTN::OTNRow row;
        while (r.ReadRow(row)) {
            // process row ...
        }
    }

    // random access, independent of the sequential position
    std::optional<OTN::OTNObject> enemy = r.ReadObjectRow("Enemy", 5000);
}
```

Object references are resolved by default (`SetResolveObjectRefs(false)` keeps the row indices).

## Custom Types

Specialize `OTN::ToOTNDataType<T>` to add serialization support for your own types:

```cpp
struct Vector2 { float x, y; };

template<>
inline void OTN::ToOTNDataType<Vector2>(OTN::OTNObjectBuilder& obj, Vector2& v) {
    obj.SetObjectName("Vector2");
    obj.AddNames("x", "y");
    obj.AddData(v.x, v.y);
}
```

Use `IsBuildingOTNObject()` to separate save and load logic when needed:

```cpp
template<>
inline void OTN::ToOTNDataType<Vector2>(OTN::OTNObjectBuilder& obj, Vector2& e) {
    obj.SetObjectName("Vector2");
    obj.AddNames("list");

    if (obj.IsBuildingOTNObject()) {
        obj.AddData(std::vector<float>{ e.x, e.y });
    } else {
        std::vector<float> tmp;
        obj.AddData(tmp);
        if (tmp.size() >= 2) { e.x = tmp[0]; e.y = tmp[1]; }
    }
}
```

`AddNameData(name, value)` adds a name and its value in one call. `AddTypes(...)` and `AddNameTypeData(name, type, value)` set column types explicitly, e.g. for empty lists or `any` columns.

> **Note:** Custom types require a default constructor. The order in which fields are added determines save/load compatibility — changing the order is a breaking change. Every object name must be unique: a custom type must not use the name of another object with different columns.

## Supported Types

| Type string | C++ type       |
|-------------|----------------|
| `int`       | `int`          |
| `int64`     | `int64_t` (`uint32_t` / `unsigned int` are stored as `int64`) |
| `uint64`    | `uint64_t`     |
| `float`     | `float`        |
| `double`    | `double`       |
| `bool`      | `bool`         |
| `String`    | `std::string`, `const char*` |
| `any`       | numbers, `std::string`, `bool`, custom types, `OTNObject` and lists of these, also mixed |
| `int[]`     | `std::vector<int>`, `std::array<int, N>` |
| `MyType`    | custom type or `OTNObject` (written as `Ref<MyType>`) |

Multi-dimensional arrays (e.g. `int[][]`) are supported by increasing the list depth.

Column types are deduced from the first row or set with `SetTypes(...)` before the first row. Numbers in `any` columns can be read as every number type the value fits into.

## Writer Options

```cpp
writer.UseDefType(true)          // replaces often used type names with numbers
      .UseDefName(true)          // replaces often used column names with numbers
      .UseOptimizations(true)    // no spaces, line breaks and indentation
      .UseDeduplicateRows(true); // identical rows are written once
```

Numbers are only used if they make the file shorter. These options are only available for `OTNWriter`.

## Building & Tests

A test project is included that covers various serialization and deserialization cases. The build system uses [Premake5](https://premake.github.io/) via a `build.bat` script on Windows.

### Generating project files

```bat
build.bat vs2022       # Visual Studio 2022
build.bat vs2019       # Visual Studio 2019
build.bat gmake        # GNU Makefiles (MinGW / Cygwin)
```

Run `build.bat help` to see all available targets.

### Compiling directly (Windows)

```bat
build.bat compile
```

This generates a Visual Studio 2022 solution and immediately builds it in Debug/x64 using `msbuild`. It uses an active Visual Studio developer environment, otherwise the newest Visual Studio installation with C++ build tools is found via `vswhere` (any version and edition).

### Running the tests

```bat
OTNFileTest.exe <output_directory> [--no-wait]
```

The test files are written into `<output_directory>`. Without `--no-wait` the program waits for a key press at the end.

### Debug checks

`OTN_DEBUG` enables additional checks for every row (column types of every row, unique names in custom types). It is on in debug builds (without `NDEBUG`) and off otherwise. Define it as `0` or `1` for all files to override it.

### Other platforms

Premake5 supports other targets (`gmake`, `xcode4`, etc.), but the project has only been tested on Windows. You may need to adjust include paths or compiler flags for other platforms.

## When to Use OTN

OTN works best with **small to medium-sized datasets**. `OTNReader` loads the entire file into memory at once and is the simplest way to read a file.

`OTNStreamWriter` and `OTNStreamReader` exist for larger files to keep memory usage low. The stream reader keeps an index of all objects and every 1024th row, rows are read from the file when they are needed, so every read and every resolved reference accesses the file. The stream writer needs disk space for the data twice while `Close()` builds the file. If you find yourself relying heavily on the stream classes, consider whether a dedicated database or binary format might be a better fit for your use case.

## Notes

- **Thread safety:** all classes are not thread-safe; use external synchronization.
- **Errors:** writers and readers do not throw on invalid data, they return `false` and report the reason via `GetError()` / `TryGetError()`.
- **Exceptions:** `GetValue<T>()` may throw `std::out_of_range` or `std::bad_cast`; use `TryGetValue<T>()` or `GetValue<T>(row, column, default)` for a non-throwing alternative.
- **Names** of objects, columns and custom types may contain letters, digits, `_` and UTF-8 characters, but must not be empty or start with a digit.
- **Numbers:** NaN and infinity cannot be written.
- **Encoding:** files are UTF-8; a UTF-8 byte order mark is skipped when reading.
- The `.otn` extension is added automatically if omitted from file paths.

## File Format

OTN is a plain-text format. The format version is stored in the file header (`@version: 2;`). Version 2 is the current format version, files of version 1 can still be read. OTNFile v1.0 does not know version 2 and may fail to read files written by this version.

The full documentation, including the file format, is in [doc/OTNFile.html](doc/OTNFile.html).

## Contributing

Contributions are welcome! Please open an issue or pull request for any improvements or bug fixes.
