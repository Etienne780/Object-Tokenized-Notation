---
id: node_mu55p95x60r14y7tsvw1
name: OTNObject
---

# OTNObject Internals

`OTNObject` holds the columns and all rows of one object. Readers and writers only work with `OTNObject`s, so this is the central data structure.

## Members

{col 0: width=190}
| Member | Type | Description |
|---|---|---|
| `m_name` | `std::string` | Object name |
| `m_columnNames` | `vector<string>` | One name per column |
| `m_columnTypes` | `vector<OTNTypeDesc>` | One type per column, empty until set or deduced |
| `m_dataRows` | `vector<OTNRow>` | All rows, a row is a `vector<OTNValue>` |
| `m_columnIndexCache` | `unique_ptr<unordered_map<string, size_t>>` | Column name to index, created and filled on the first lookup |
| `m_deducedColumnTypes` | `bool` | Column types are known (set or deduced) |
| `m_wasBuildFromT` | `bool` | Created by `OTNObjectBuilder` from a custom type |
| `m_valid`, `m_error` | `bool`, `string` | Error state |

## Adding rows

`AddDataRow(args...)` converts every argument with `Detail::OTNDataType<T>` (see [[type-conversion]]) into an `OTNValue`. If one value fails, the row is not added and the object becomes invalid.

After the first row, `DeduceTypesFromRow` sets the column types that were not given with `SetTypes()`. `SetTypes` parses the type strings with `Detail::ParseTypeString`, the same function the readers use, see [[types]].

## Checks

Always active:

- `IsNameValid`: object, column and type names. Uses `Detail::IsNameStartChar` / `IsNameChar`, the same functions as the `Tokenizer`, so every valid name can be read again.
- `ValidateNamesDistinct`: no column name is used twice.
- `SetNames` / `SetTypes` before the first row, one value per column in `AddDataRow`.

Only with `OTN_DEBUG` (costs time for every row):

- `ValidateRowTypes`: every row has the types of its columns.

`OTNObjectBuilder::ToOTNObject` calls `ValidateRowTypes(0)` always if the custom type set types with `AddTypes`.

## Column lookup

`GetColumnID(name)` searches the column names and stores the result in `m_columnIndexCache`. The cache is cleared by `SetNames` and by copying, because it belongs to one object. It is only created on the first lookup, so objects that are never searched by name (for example resolved references) need no allocation for it.

## Reading values

`GetValue<T>` and `TryGetValue<T>` call `TryDeserializeValueImpl<T>(value, allowNumericConversion)`. The second argument is `IsAnyColumn(column)`: only in `any` columns numbers are converted to other number types (`Detail::OTNConvertNumber`, range checked). In all other columns the stored type must be `T`.

`GetValue<T>` without default throws `std::out_of_range` or `std::bad_cast`, the other variants return the default or `std::nullopt`.

## Objects as values

A value of type `OBJECT` holds a `std::shared_ptr<OTNObject>`. `m_wasBuildFromT` decides how a writer references it:

- Built from a custom type: always one row, referenced by one index (`Ref<X>`).
- Normal `OTNObject`: all its rows are referenced (`Ref<X>[]`).
