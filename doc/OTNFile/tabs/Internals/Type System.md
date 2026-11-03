---
id: node_mu55p95x16kjtnh1dqih2o
name: Type System
---

# Type System

How values and column types are represented in memory. The type strings used in the API are listed in [[types]].

## OTNBaseType

```cpp
enum class OTNBaseType {
    UNKNOWN = 0,
    INT, UINT64, INT64, FLOAT, DOUBLE, BOOL, STRING,
    OBJECT,     // nested OTNObject
    OBJECT_REF, // reference not resolved yet (name + row index)
    LIST,       // list of values
    ANY         // only used as column type
};
```

`OBJECT_REF` values are created while reading. `OTNReader` replaces all of them by `OBJECT` values. `OTNStreamReader` does the same unless `SetResolveObjectRefs(false)` is set.

## OTNTypeDesc

Describes the type of a column:

```cpp
struct OTNTypeDesc {
    OTNBaseType baseType = OTNBaseType::UNKNOWN;
    uint32_t listDepth = 0;    // 0 = T, 1 = T[], 2 = T[][]
    std::string refObjectName; // object name if baseType == OBJECT
};
```

| Type string | baseType | listDepth | refObjectName |
|---|---|---|---|
| `int` | `INT` | 0 | |
| `float[][]` | `FLOAT` | 2 | |
| `Vector2` | `OBJECT` | 0 | `Vector2` |
| `Vector2[]` | `OBJECT` | 1 | `Vector2` |

`UNKNOWN` with `listDepth > 0` is the deduced type of an empty list. Such a type cannot be written.

## OTNValue

A value is a variant plus its type:

```cpp
using OTNValueVariant = std::variant<
    int, int64_t, uint64_t, float, double, bool, std::string,
    Detail::OTNObjectPtr,  // std::shared_ptr<OTNObject>
    Detail::OTNArrayPtr,   // std::shared_ptr<Detail::OTNArray>
    Detail::OTNObjectRef   // { std::string refObjectName; size_t index; }
>;

struct OTNValue {
    OTNValueVariant value;
    OTNBaseType type; // set from the variant
};

using OTNRow = std::vector<OTNValue>;
```

`Detail::OTNArray` holds a `std::vector<OTNValue>`. Nested lists are arrays in arrays.

## Public and internal types

Everything a user needs is in the namespace `OTN`: the classes, `OTNValue`, `OTNValueVariant`, `OTNRow`, `OTNTypeDesc`, `OTNBaseType`, `OTNColumnDef` / `col()`, the constants (`VERSION`, `FILE_EXTENSION`, `TEMP_FILE_EXTENSION`) and `Syntax`.

Implementation details are in `OTN::Detail` and should not be used directly:

| In OTNFile.h | Purpose |
|---|---|
| `OTNArray`, `OTNObjectPtr`, `OTNArrayPtr`, `OTNObjectRef` | Parts of `OTNValueVariant` |
| `OTNDataType<T>` | Converts C++ values into `OTNValue`, see [[type-conversion]] |
| `OTNTypeOf<T>`, `GetType`, `GetTypeFromVariant` | Base type of a C++ type / variant |
| `is_otn_base_type_v`, `is_otn_list_v`, `otn_is_std_vector`, `is_otn_number_v` | Traits for `if constexpr` dispatch |
| `OTNNumericCast`, `OTNConvertNumber` | Range checked number conversion for `any` columns |
| `FileHeaderInfo`, `ObjectHeaderInfo`, `DefTable`, `WriteOptions`, `FilePosition` | Data exchanged with the syntax layer, see [[syntax-layer]] |

The parser and the syntax functions are only declared in the header (`CharSource`, `Tokenizer`, `ReadSyntax`) and defined in `OTNFile.cpp`.
