---
id: node_mu55p95xcc3t1kyr59wt
name: Type Conversion
---

# Type Conversion

The conversion pipeline is the bridge between raw C++ values and `OTNValue` instances stored in `OTNObject` rows. It runs in both directions: serialization (C++ → `OTNValue`) and deserialization (`OTNValue` → C++).

## Serialization: C++ → OTNValue

`OTNDataType<T>` is the entry point for converting a C++ value into an `OTNValue`. It is instantiated every time a value is added to a row.

```
AddDataRow(v.x, v.y)
    → AddSingleValueToRow(row, v.x, ...)
        → OTNDataType<float>(v.x)
            → m_value = v.x   (is_otn_base_type_v<float> = true)
```

The `OTNDataType::Construct` method dispatches on type traits:

| Input type | Dispatch path |
|---|---|
| Primitive (`int`, `float`, `bool`, `std::string`, …) | Stored directly in the variant |
| `const char*` | Converted to `std::string`, then stored |
| `uint32_t` / `unsigned int` | Widened to `int64_t` (no `uint32_t` slot in the variant) |
| `std::vector<T>` / `std::array<T,N>` | Creates an `OTNArray`; each element recursively constructed via `OTNDataType<elem_type>` |
| `OTNObject` | Wrapped in a `shared_ptr<OTNObject>` and stored as `OBJECT` |
| Any other type `T` | Calls `ToOTNDataType<T>` via `OTNObjectBuilder` (custom type path) |

For custom types, an `OTNObjectBuilder` is constructed in serialization mode, `ToOTNDataType` is called, and the resulting `OTNObject` is boxed into a `shared_ptr` and stored as `OBJECT`.

## Deserialization: OTNValue → C++

`OTNObject::TryDeserializeValue<T>` converts an `OTNValue` back to a C++ type `T`. It is called by all `GetValue` / `TryGetValue` overloads.

```
TryGetValue<Vector2>(row, "position")
    → TryDeserializeValue<Vector2>(val)
        → val.type == OBJECT
        → ptr->GetRowCount() == 1
        → OTNObjectBuilder{ *ptr }  (deserialization mode)
        → ToOTNDataType<Vector2>(builder, result)
        → result is filled
```

Dispatch in `TryDeserializeValue`:

| Target type `T` | Condition | Action |
|---|---|---|
| Base primitive (`int`, `float`, `bool`, `std::string`, …) | `is_otn_base_type_v<T>` | `std::get<T>(val.value)` — throws on mismatch, returns nullopt via catch |
| `std::vector<ElemType>` | `otn_is_std_vector<T>` | Iterates `OTNArray::values`, recursively deserializes each element |
| `OTNObject` | `std::is_same_v<T, OTNObject>` | Dereferences the `OTNObjectPtr` |
| Custom type `T` | fallback | Requires `val.type == OBJECT` **and** `GetRowCount() == 1`; creates `OTNObjectBuilder` in deserialization mode and calls `ToOTNDataType` |

### The one-row requirement for custom types

A nested `OTNObject` can only be deserialized into a single custom struct if it has **exactly one row**. If the object has more than one row, `TryDeserializeValue` returns `std::nullopt` for a scalar `T`. In that case, read the column as `std::vector<T>` instead. See the Reading Data section for a detailed explanation.

## OTNObjectBuilder deserialization mode

When `OTNObjectBuilder` is constructed with an `OTNObject` (deserialization mode), `AddData(T& ref)` reads from the object's first row sequentially by position. Each call to `AddData` advances an internal cursor `m_dataPos`. This means the order of `AddData` calls in `ToOTNDataType` must exactly match the column order used during serialization.

```
Deserialization call sequence for Vector2:
  obj.AddNames("x", "y")  → no-op in deserialization mode
  obj.AddData(e.x)        → reads m_data[0] into e.x, m_dataPos = 1
  obj.AddData(e.y)        → reads m_data[1] into e.y, m_dataPos = 2
```

## Type deduction

When `SetTypes()` is not called, column types are deduced automatically from the **first row** inserted via `AddDataRow`. `DeduceTypesFromRow` calls `OTNWriter::DeduceColumnType` on each `OTNValue` in that row, which inspects the variant index and the object name for `OBJECT` types. Subsequent rows are validated in debug builds to ensure type consistency.
