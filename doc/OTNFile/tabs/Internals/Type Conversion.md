---
id: node_mu55p95xcc3t1kyr59wt
name: Type Conversion
---

# Type Conversion

C++ values are converted into `OTNValue`s when they are added to a row, and back when they are read.

## C++ to OTNValue

`Detail::OTNDataType<T>` converts one value. It is used by `OTNObject::AddDataRow`, `OTNObjectBuilder::AddData` and `OTNStreamWriter::WriteRow`.

```text
AddDataRow(1, Vector2{ 1, 2 })
    OTNDataType<int>(1)            -> int
    OTNDataType<Vector2>(v)        -> OTNObjectBuilder + ToOTNDataType<Vector2>
                                   -> shared_ptr<OTNObject> (one row)
```

| C++ type | Stored as |
|---|---|
| `int`, `int64_t`, `uint64_t`, `float`, `double`, `bool`, `std::string` | The same type |
| `const char*`, string literal | `std::string` |
| `uint32_t`, `unsigned int` | `int64_t` |
| `std::vector<T>`, `std::array<T, N>` | `Detail::OTNArray`, every element converted with `OTNDataType` |
| `OTNObject` | Copy in a `shared_ptr<OTNObject>` |
| Any other `T` | `ToOTNDataType<T>` through an `OTNObjectBuilder`, see [[object-builder]] |

Errors (for example an invalid custom type) set `m_valid = false` in the `OTNDataType`, the row is then rejected.

## OTNValue to C++

`OTNObject::TryDeserializeValueImpl<T>(value, allowNumericConversion)` converts back:

| Target `T` | Conversion |
|---|---|
| Base type | `std::get_if<T>`. In `any` columns numbers are converted with `Detail::OTNConvertNumber<T>` (range checked). |
| `uint32_t`, `unsigned int` | From the stored `int64_t`, range checked |
| `std::vector<E>` | Every element of the `OTNArray` converted to `E` |
| `OTNObject` | Copy of the referenced object |
| Custom type | `OTNObjectBuilder` in load mode, the object must have exactly one row |

`allowNumericConversion` is `true` only for `any` columns, so a `float` column cannot be read as `double` by mistake.

## Number conversion

`Detail::OTNNumericCast<To>(from)` returns `std::nullopt` if the value does not fit:

- integer to integer: outside the range of `To`,
- floating point to integer: not finite, has decimals or outside the range,
- to floating point: always possible (may round).

## Lists of objects

An `OTNObject` with several rows can only be read as `std::vector<T>`. In memory the value is a list with one `OTNObject` per row after reading, because the file stores a list of row indices (see [[writing-data]]).
