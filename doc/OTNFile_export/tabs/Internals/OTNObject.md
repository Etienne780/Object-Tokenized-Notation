---
id: node_mu55p95x60r14y7tsvw1
name: OTNObject
---

# OTNObject Internals

`OTNObject` is the central data structure. It holds the schema (names + types) and all data rows for one logical table.

## Internal state

| Member | Type | Description |
|---|---|---|
| `m_name` | `std::string` | Object identifier; written as the object name in the file |
| `m_columnNames` | `vector<string>` | Column names, one per column |
| `m_columnTypes` | `vector<OTNTypeDesc>` | Type descriptor per column; may be empty until deduced |
| `m_dataRows` | `vector<OTNRow>` | All data rows; each row is a `vector<OTNValue>` |
| `m_columnIndexCache` | `unordered_map<string, size_t>` | Lazy-built name→index cache for fast column lookup |
| `m_deducedColumnTypes` | `bool` | True after types have been deduced from the first row |
| `m_valid` | `bool` | False if any error occurred during building |
| `m_wasBuildFromT` | `bool` | True if the object was created via `OTNObjectBuilder` (custom type path) |

## Column name validation

Names are checked in debug builds by `DebugIsNameValid`. A name must not start with a digit and must not contain any OTN syntax characters (`@`, `:`, `=`, `,`, `/`, `{`, `}`, `[`, `]`, `<`, `>`, `;`). Duplicate names are also caught by `DebugValidateNamesDistinct`.

## Type deduction

`DeduceTypesFromRow` is called automatically after the first `AddDataRow` if `SetTypes()` was never called. It derives `OTNTypeDesc` from each `OTNValue` by inspecting the variant. For `OBJECT`-typed values it also captures the `refObjectName` from the embedded `OTNObject`. Once deduced, `m_deducedColumnTypes` is set to `true` and subsequent rows are type-checked in debug builds by `DebugValidateDataTypes`.

## Column lookup

`GetColumnID(name)` looks up a column index by name. Results are cached in `m_columnIndexCache` after the first lookup. The cache is invalidated (cleared) when `SetNames()` is called.

## Row access

`GetRow(index)` is bounds-checked and throws `std::out_of_range` on invalid indices. `GetValue<T>` and `TryGetValue<T>` both delegate to `TryDeserializeValue<T>` internally; the difference is that `GetValue<T>` without a default re-throws from `std::get` on type mismatch, while `TryGetValue<T>` catches all exceptions and returns `std::nullopt`.

## Error state

Any error sets `m_valid = false` and appends a message to `m_error`. Errors do not throw — the object silently enters an invalid state that can be queried via `IsValid()`, `GetError()`, or `TryGetError()`. Invalid objects should not be passed to `OTNWriter`.
