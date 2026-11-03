---
id: node_w3f6j0p8u2lcy5mx9tbe
name: Types
---

# Types

Every column has a type. It is deduced from the first row or set with `SetTypes()` (`OTNObject`) and `BeginObject()` (`OTNStreamWriter`).

## Type strings

{hover=row; col 0: width=170}
| Type string | C++ type | Note |
|---|---|---|
| `int` | `int` | |
| `int64` | `int64_t` | `uint32_t` / `unsigned int` values are stored as `int64` |
| `uint64` | `uint64_t` | |
| `float` | `float` | |
| `double` | `double` | |
| `bool` | `bool` | |
| `String` | `std::string` | `const char*` and string literals are stored as `String` |
| `any` | number, `std::string`, `bool`, custom type, `OTNObject` | The type is not fixed, see below |
| `Name` | custom type or `OTNObject` | Reference to the object `Name`, see [[custom-types]] |
| `T[]` | `std::vector<T>`, `std::array<T, N>` | One `[]` per nesting level: `int[][]` |

`Name` is any other name, for example `"Vector2"` or `"Vector2[]"`. In the file it is written as `Ref<Vector2>`.

## Deduced types

Without `SetTypes()` the type of every column is taken from the value in the first row:

```cpp
obj.SetNames("a", "b", "c", "d")
   .AddDataRow(1, "text", std::vector<float>{ 1.0f }, Vector2{});
// int, String, float[], Vector2
```

An empty list has no element type. The writers then take the type from the next row with a non empty list. If the list is empty in all rows, set the type with `SetTypes()` (or `AddTypes()` in a custom type).

## Reading values

`GetValue<T>` and `TryGetValue<T>` only succeed if `T` is the stored type. `int` cannot be read as `int64_t` or `float`, `float` not as `double`.

`uint32_t` and `unsigned int` are read back from the stored `int64` if the value fits.

## any

An `any` column can hold numbers, strings, `bool`, custom types, `OTNObject`s and lists of these, also mixed:

```cpp
obj.SetNames("value").SetTypes("any")
   .AddDataRow(3)
   .AddDataRow(0.25)
   .AddDataRow(std::string("text"))
   .AddDataRow(Vector2{ 1, 2 })
   .AddDataRow(std::vector<Vector2>{ { 3, 4 }, { 5, 6 } });
```

Values are read with the type they should have:

```cpp
int i       = obj.GetValue<int>(0, "value", 0);
auto pos    = obj.TryGetValue<Vector2>(3, "value");
auto path   = obj.TryGetValue<std::vector<Vector2>>(4, "value");
```

Custom types and `OTNObject`s are written as rows of their own object, the `any` value references them with `Name#index` (see [[file-format]]). An `OTNObject` with several rows becomes a list of references and is read as `std::vector<OTNObject>`.

In the file a number has no type. When reading, it gets the smallest type that fits: `int`, then `int64_t`, then `uint64_t`, and `double` for numbers with a decimal point or exponent.

So that the type used for writing does not matter, numbers in `any` columns can be read as **every number type** if the value fits:

| Stored | Read as | Result |
|---|---|---|
| `3` | `int`, `int64_t`, `float`, `double`, … | `3` |
| `0.1f` | `float` | `0.1f` |
| `3.5` | `int` | fails, decimals would be lost |
| `5000000000` | `int` | fails, too large |
| `-1` | `uint64_t` | fails, negative |

Floating point numbers are written with full precision, so a `float` or `double` is read back exactly.

## Names

Object names, column names and custom type names may contain:

- letters `a-z`, `A-Z`, digits `0-9` and `_`,
- UTF-8 characters (all bytes from `0x80`), for example `Größe`.

A name must not be empty and must not start with a digit. Spaces and syntax characters (`@ : = , / { } [ ] < > ; "`) are not allowed. An invalid name makes the object invalid.

## Text encoding

Files are UTF-8. A UTF-8 byte order mark at the start of a file is skipped when reading, the writers never write one. UTF-16 and UTF-32 files are rejected with an error.

`std::string` values are written byte by byte. The library does not check or convert their encoding, so they are read back exactly as they were written. `std::wstring`, `std::u16string` and similar types are not supported directly. Store them as UTF-8 `std::string` or with your own `ToOTNDataType` specialization.
