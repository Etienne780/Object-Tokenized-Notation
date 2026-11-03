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
| `any` | number, `std::string`, `bool` | The type is not fixed, see below |
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

An empty list in the first row has no element type. In that case set the type with `SetTypes()`.

## Reading values

`GetValue<T>` and `TryGetValue<T>` only succeed if `T` is the stored type. `int` cannot be read as `int64_t` or `float`, `float` not as `double`.

`uint32_t` and `unsigned int` are read back from the stored `int64` if the value fits.

## any

An `any` column can hold numbers, strings, `bool` and lists of these, also mixed:

```cpp
obj.SetNames("value").SetTypes("any")
   .AddDataRow(3)
   .AddDataRow(0.25)
   .AddDataRow(std::string("text"));
```

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
