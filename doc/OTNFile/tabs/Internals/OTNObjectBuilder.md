---
id: node_mu55p95x1mr9l481ystmfe
name: OTNObjectBuilder
---

# OTNObjectBuilder

`OTNObjectBuilder` lets the same `ToOTNDataType<T>` specialization save and load a custom type. The constructor decides the direction.

```cpp
OTNObjectBuilder builder;          // saving:  T -> OTNObject
OTNObjectBuilder builder{ obj };   // loading: OTNObject -> T
```

`IsBuildingOTNObject()` returns `true` when saving.

## Saving (T to OTNObject)

Used by `Detail::OTNDataType<T>` for every value of a custom type:

1. A builder is created and `ToOTNDataType<T>(builder, value)` is called.
2. `SetObjectName` stores the object name, `AddNames` the column names (must be unique).
3. `AddData` converts every member with `Detail::OTNDataType` into an `OTNValue`. Custom members create their own builder (recursion).
4. `std::move(builder).ToOTNObject()` creates an `OTNObject` with one row and `m_wasBuildFromT = true`.

## Loading (OTNObject to T)

Used by `OTNObject::TryDeserializeValue<T>` when `T` is a custom type:

1. The constructor copies the only row of the object into `m_data`. An object with zero or more than one row is an error.
2. `ToOTNDataType<T>(builder, result)` is called on a default constructed `T`.
3. `SetObjectName` checks that the name matches the stored object.
4. `AddNames` does nothing.
5. Every `AddData(member)` takes the value at `m_dataPos`, converts it into the member type and moves `m_dataPos` forward.

Because values are taken by position, the order of `AddData` calls must be the same for saving and loading.

## AddNameData

`AddNameData(name, value)` calls `AddNames` and `AddData` for one column. It checks that the number of names and values is still the same (`IsDataOutOfSync`), which finds missing names or values in a specialization.

## Errors

All errors are collected in the builder (`IsValid`, `GetError`). When saving, they make the value and therefore the row invalid. When loading, `TryGetValue` returns `std::nullopt`.
