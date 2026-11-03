---
id: node_h4t9b1s6e3xan7pk2rwc
name: Error Handling
---

# Error Handling

The library does not use exceptions to report errors. Every class has an error state instead:

| Method | Returns |
|---|---|
| `IsValid()` | `false` after an error |
| `GetError()` | All error messages, separated by line breaks |
| `TryGetError(out)` | `true` and the messages in `out` if there was an error |

Functions that can fail (`Save`, `ReadFile`, `Open`, `WriteRow`, …) also return `false` or `std::nullopt`.

```cpp
OTN::OTNReader reader;
if (!reader.ReadFile("data.otn"))
    std::cerr << reader.GetError() << '\n';
```

Read errors contain the position in the file:

```text
Error at line 7, column 12: unexpected token IDENTIFIER("abc"), expected ';'
```

## After an error

After an error all following calls fail until the state is reset:

| Class | Reset |
|---|---|
| `OTNObject` | Create a new object |
| `OTNWriter` | `Reset()` removes the error and all objects, the options stay |
| `OTNReader` | Every `ReadFile` / `ReadString` starts with a new state |
| `OTNStreamWriter` | `Open()` starts with a new state |
| `OTNStreamReader` | `Open()` starts with a new state |

```cpp
OTN::OTNWriter writer;
if (!writer.AppendObject(obj).Save("data.otn")) {
    std::cerr << writer.GetError() << '\n';
    writer.Reset(); // ready for the next file
}
```

`OTNWriter` is not reset by `Save()` itself, so an error of `AppendObject` (for example a name that is used twice) is still reported by the next `Save()`.

## OTNObject

Adding a row with the wrong number of values, an invalid custom type or invalid names makes the object invalid. Check it before writing:

```cpp
if (!obj.IsValid())
    std::cerr << obj.GetError() << '\n';
```

These checks always run:

- object and column names are valid (see [[types]]) and column names are unique,
- `SetNames` / `SetTypes` are called before the first row,
- every row has one value per column,
- `OTNWriter` gets no invalid objects and no two objects with the same name,
- explicit types of custom types (`AddTypes`) match the values.

## OTN_DEBUG

Some checks cost time for every row or value, so they only run if `OTN_DEBUG` is `1`:

- every row has the types of its columns,
- the column names of a custom type are unique.

By default `OTN_DEBUG` is `1` in debug builds (without `NDEBUG`) and `0` otherwise. To change it, define it in the build system, for example in premake:

```lua
defines { "OTN_DEBUG=1" }
```

The value must be the same for all files of the program, because `OTNFile.h` and `OTNFile.cpp` both use it.

## Exceptions

Only reading values can throw:

- `GetValue<T>(row, column)` throws `std::out_of_range` if the row or column does not exist and `std::bad_cast` if the value does not have the type `T`.
- `GetRow(index)` throws `std::out_of_range`.

`GetValue<T>(row, column, default)` and `TryGetValue<T>(row, column)` never throw.

File system errors are reported as normal errors: missing folders and files, missing rights, failed reads and writes, also when the file is closed (for example a full disk). Only running out of memory (`std::bad_alloc`) is not caught.
