---
id: node_mu55p95x1mr9l481ystmfe
name: OTNObjectBuilder
---

# OTNObjectBuilder

`OTNObjectBuilder` is the runtime bridge that makes the same `ToOTNDataType<T>` specialization work for both serialization and deserialization.

## Two modes

The mode is determined by which constructor is used:

```cpp
OTNObjectBuilder builder; // serialization mode  (T → OTNObject)
OTNObjectBuilder builder{ obj }; // deserialization mode (OTNObject → T)
```

`IsBuildingOTNObject()` returns `true` in serialization mode.

## Serialization mode (T → OTNObject)

- `SetObjectName(name)` — stores the object name.
- `AddNames("x", "y", ...)` — records column names for the output object.
- `AddData(v.x, v.y, ...)` — each value is wrapped in `OTNDataType<U>` and appended to `m_data`. For custom types this recurses, calling `ToOTNDataType` again.
- `ToOTNObject()` — consumes the builder (rvalue-only) and returns an `OTNObject` built from the collected names and data. Called internally after `ToOTNDataType` returns.

## Deserialization mode (OTNObject → T)

The constructor `OTNObjectBuilder(const OTNObject& obj)` copies the first row of `obj` into `m_data` and sets `m_otnObjectFromT = false`.

- `SetObjectName(name)` — checks that the provided name matches the source object name. Reports an error on mismatch (but does not abort).
- `AddNames(...)` — no-op; column positions are used, not names.
- `AddData(ref)` — reads `m_data[m_dataPos]` and writes the value into `ref` via `DeserializeFromOTNValue`. Advances `m_dataPos`.

`DeserializeFromOTNValue<T>` handles base types (`std::get<T>`), lists (iterates `OTNArray`), `OTNObject` values (dereferences pointer), and custom types (creates a nested builder and recurses).

## AddNameData

`AddNameData(name, data)` is a convenience that combines one `AddNames` and one `AddData` call atomically, with a sync check. It is useful when the name and value for a column are available together.

## Data sync validation

In serialization mode, `AddName` ensures each name is unique. `IsDataOutOfSync()` is checked before `AddNameData` to verify that the number of names and data values match — an out-of-sync state indicates a programming error in the `ToOTNDataType` specialization.
