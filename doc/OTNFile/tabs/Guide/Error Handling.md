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

## Errors are permanent

After an error an object stays invalid, all following calls fail as well:

| Class | Reset |
|---|---|
| `OTNObject` | Create a new object |
| `OTNWriter` | Create a new writer |
| `OTNReader` | Create a new reader |
| `OTNStreamWriter` | `Open()` starts with a new state |
| `OTNStreamReader` | `Open()` starts with a new state |

## OTNObject

Adding a row with the wrong number of values, an invalid custom type or wrong column names makes the object invalid. Check it before writing:

```cpp
if (!obj.IsValid())
    std::cerr << obj.GetError() << '\n';
```

Some checks only run in **debug builds** (without `NDEBUG`), because they cost time for every row:

- column names are valid and unique,
- every row has the same types as the first row,
- `OTNWriter` gets no invalid objects and no two objects with the same name.

In release builds these mistakes are not detected and can produce files that cannot be read.

## Exceptions

Only reading values can throw:

- `GetValue<T>(row, column)` throws `std::out_of_range` if the row or column does not exist and `std::bad_cast` if the value does not have the type `T`.
- `GetRow(index)` throws `std::out_of_range`.

`GetValue<T>(row, column, default)` and `TryGetValue<T>(row, column)` never throw.

File system errors (missing rights, full disk) are reported as normal errors.
