---
id: node_mu55p95x16kjtnh1dqih2o
name: Type System
---

# Type System

OTN's type system maps C++ types to a small set of base types stored in the file. Everything in the system ultimately resolves to an `OTNBaseType` enum value.

## OTNBaseType

```cpp
enum class OTNBaseType {
    UNKNOWN = 0,
    INT,        // int (32-bit signed)
    UINT64,     // uint64_t
    INT64,      // int64_t
    FLOAT,      // float
    DOUBLE,     // double
    BOOL,       // bool
    STRING,     // std::string
    OBJECT,     // nested OTNObject (resolved pointer)
    OBJECT_REF, // unresolved integer reference (internal, during parsing)
    LIST,       // OTNArray — a sequence of OTNValues
    ANY         // untyped — type is stored inline per value
};
```

`OBJECT_REF` only exists transiently during file parsing. After `OTNReader` finishes reading, all `OBJECT_REF` values are resolved to `OBJECT` values (actual `OTNObjectPtr` pointers). User code never sees `OBJECT_REF` unless it uses `OTNStreamReader`, where references remain unresolved by design.

## OTNTypeDesc

`OTNTypeDesc` describes a column type, including array nesting depth and optional object name.

```cpp
struct OTNTypeDesc {
    OTNBaseType baseType = OTNBaseType::UNKNOWN;
    uint32_t listDepth = 0; // 0 = scalar, 1 = T[], 2 = T[][], ...
    std::string refObjectName; // only used when baseType == OBJECT
};
```

Examples:

| C++ type | baseType | listDepth | refObjectName |
|---|---|---|---|
| `int` | `INT` | 0 | — |
| `std::vector<int>` | `INT` | 1 | — |
| `std::vector<std::vector<float>>` | `FLOAT` | 2 | — |
| `MyStruct` | `OBJECT` | 0 | `"MyStruct"` |
| `std::vector<MyStruct>` | `OBJECT` | 1 | `"MyStruct"` |

## Type string parsing

Type strings like `"int"`, `"float[]"`, or `"WeaponStats"` are parsed by `StringToOTNBaseType`. If the string does not match any known primitive, it is treated as an object type name and `baseType` is set to `OBJECT` with `refObjectName` set to the string value. Array brackets `[]` increase `listDepth` by one per pair.

## OTNValueVariant

All runtime values are stored as a `std::variant`:

```cpp
using OTNValueVariant = std::variant<
    int,
    int64_t,
    uint64_t,
    float,
    double,
    bool,
    std::string,
    OTNObjectPtr, // shared_ptr<OTNObject>
    OTNArrayPtr, // shared_ptr<OTNArray>
    OTNObjectRef // integer ref — internal only
>;
```

`OTNArray` is a simple container:

```cpp
class OTNArray {
public:
    std::vector<OTNValue> values;
};
```

Nested lists are represented by nesting `OTNArray` inside `OTNArray` — an `OTNValue` of type `LIST` whose inner values are also `LIST`-typed.

## OTNValue

`OTNValue` wraps a `OTNValueVariant` together with a cached `OTNBaseType` tag for fast type checks without calling `std::visit`:

```cpp
struct OTNValue {
    OTNValueVariant value;
    OTNBaseType     type = OTNBaseType::UNKNOWN;
};
```

The `type` field is set automatically from the variant on construction via `GetTypeFromVariant`.

## Type traits

Several `constexpr` traits drive compile-time dispatch:

| Trait | Purpose |
|---|---|
| `is_otn_base_type_v<T>` | True for all primitive types OTN can store directly (`int`, `float`, `std::string`, `const char*`, etc.) |
| `is_otn_list_v<T>` | True for `std::vector<T>` and `std::array<T,N>` |
| `otn_is_std_vector<T>` | True only for `std::vector<T>` (used in deserialization) |
| `is_otn_variant_v<T>` | True when T is exactly `OTNValueVariant` |
| `OTNTypeOf<T>::value` | Maps a C++ type to its `OTNBaseType` at compile time |

These traits are used throughout `OTNDataType`, `AddSingleValueToRow`, and `TryDeserializeValue` to pick the correct code path without runtime branching.
