# OTNFile — Object Tokenized Notation

A lightweight, header-based C++17 library for structured data serialization using a custom `.otn` text format.

## Features

- **Tabular data model** — data is organized in named objects with typed columns and rows, similar to a dataframe or a database table
- **Type-safe** — supports `int`, `int64`, `uint64`, `float`, `double`, `bool`, `String`, nested objects, and multi-dimensional arrays
- **Custom type support** — extend serialization to your own types via template specialization
- **Two read modes:**
  - `OTNReader` — loads the entire file into memory for random access
  - `OTNStreamReader` — reads objects and rows one at a time with O(1) memory usage, suited for large files
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
writer.AppendObject(obj)
      .Save("data.otn");
```

### Reading

```cpp
OTN::OTNReader reader;
if (reader.ReadFile("data.otn")) {
    auto obj = reader.TryGetObject("Items");
    if (obj) {
        int         id   = obj->GetValue<int>(0, "id");
        std::string name = obj->GetValue<std::string>(0, "name");

        // With fallback default
        float value = obj->GetValue<float>(0, "value", 0.0f);

        // As optional
        std::optional<int> maybeId = obj->TryGetValue<int>(0, "id");
    }
}
```

### Streaming large files

```cpp
OTN::OTNStreamReader r;
if (r.Open("data.otn")) {
    while (r.NextObject()) {
        std::cout << r.GetCurrentObjectName() << "\n";
        OTN::OTNRow row;
        while (r.ReadRow(row)) {
            // process row ...
        }
    }
}
```

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

> **Note:** Custom types require a default constructor. The order in which fields are added determines save/load compatibility — changing the order is a breaking change.

## Supported Types

| Type string | C++ type       |
|-------------|----------------|
| `int`       | `int`          |
| `int64`     | `int64_t`      |
| `uint64`    | `uint64_t`     |
| `float`     | `float`        |
| `double`    | `double`       |
| `bool`      | `bool`         |
| `String`    | `std::string`  |
| `int[]`     | `std::vector<int>` |
| `MyType`    | custom object  |

Multi-dimensional arrays (e.g. `int[][]`) are supported by increasing the list depth.

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

This generates a Visual Studio 2022 solution and immediately builds it in Debug/x64 using `msbuild`. It requires either an active Visual Studio developer environment or Visual Studio 2022 Community installed at the default path.

### Other platforms

Premake5 supports other targets (`gmake`, `xcode4`, etc.), but the project has only been tested on Windows. You may need to adjust include paths or compiler flags for other platforms.

## When to Use OTN

OTN works best with **small to medium-sized datasets**. The recommended `OTNReader` loads the entire file into memory at once, giving you full random access and all available features.

The `OTNStreamReader` exists for larger files to keep memory usage low, but comes with trade-offs: objects must be read **sequentially** (no random access), unresolved object references, and no ability to jump to a specific row or object. If you find yourself relying heavily on the stream reader, consider whether a dedicated database or binary format might be a better fit for your use case.

## Notes

- **Thread safety:** all classes are not thread-safe; use external synchronization.
- **Exceptions:** `GetValue<T>()` may throw `std::out_of_range` or `std::bad_cast`; use `TryGetValue<T>()` for a non-throwing alternative. File operations may throw `std::filesystem` exceptions.
- **Object names** must not start with a digit and must not contain special characters.
- The `.otn` extension is added automatically if omitted from file paths.

## File Format

OTN is a plain-text format. Version 1 is the current format version, stored in the file header.

--

## Contributing

Contributions are welcome! Please open an issue or pull request for any improvements or bug fixes.
