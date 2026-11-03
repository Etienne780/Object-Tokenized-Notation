---
id: node_mu55p95x1j1aoqjvokje3
name: Reading Data
---

# Reading Data

`OTNReader` reads a complete file into memory. After reading, all objects are available by name and all references between objects are resolved. For large files use the [[stream-reader]].

## Reading a file

```cpp
OTN::OTNReader reader;
if (!reader.ReadFile("data.otn")) {
    std::cerr << reader.GetError() << '\n';
    return;
}
```

`ReadString(text)` reads from a string instead of a file. `GetVersion()` returns the format version of the file.

If anything in the file is invalid, reading fails. Do not use the objects of a failed read, they can be incomplete. The error message contains the line and column of the problem, see [[error-handling]].

## Getting objects

```cpp
std::optional<OTN::OTNObject> items = reader.TryGetObject("Items");
if (!items) {
    // no object with this name
}
```

`TryGetObject` returns a copy. To work on the objects without copying, use `GetObjects()`:

```cpp
for (const auto& [name, object] : reader.GetObjects()) {
    // ...
}
```

## Reading values

Values are read by row index and column name or column index. There are three variants:

{hover=row; col 0: width=230}
| Call | On error |
|---|---|
| `GetValue<T>(row, column)` | Throws `std::out_of_range` (row or column missing) or `std::bad_cast` (wrong type) |
| `GetValue<T>(row, column, default)` | Returns `default` |
| `TryGetValue<T>(row, column)` | Returns `std::nullopt` |

```cpp
int id                          = items->GetValue<int>(0, "id");
float value                     = items->GetValue<float>(0, "value", 0.0f);
std::optional<std::string> name = items->TryGetValue<std::string>(0, 1);
```

Iterating all rows:

```cpp
for (size_t row = 0; row < items->GetRowCount(); ++row) {
    int id = items->GetValue<int>(row, "id", -1);
}
```

`GetColumnNames()`, `GetColumnTypesDesc()` and `GetColumnCount()` describe the columns of an object.

## The type has to match

`T` must be the type of the column. A `float` column can only be read as `float`, not as `double` or `int`.

The exception are `any` columns. They store numbers without a fixed type, so a number from an `any` column can be read as every number type, as long as the value fits (`3.5` cannot be read as `int`, `5000000000` not as `int` but as `int64_t`). See [[types]].

## Lists

A list column is read as `std::vector`:

```cpp
auto tags = items->TryGetValue<std::vector<int>>(0, "tags");
auto grid = items->TryGetValue<std::vector<std::vector<float>>>(0, "grid");
```

## Nested objects

Columns that reference other objects are read as the referenced type:

```cpp
auto pos  = player->TryGetValue<Vector2>(0, "pos");                // custom type
auto path = player->TryGetValue<std::vector<Vector2>>(0, "path"); // list of custom types
auto inv  = player->TryGetValue<std::vector<OTN::OTNObject>>(0, "inventory");
```

An `OTNObject` with more than one row was written as a list of references (`Ref<Item>[]`, see [[writing-data]]). Such a column must be read as `std::vector<OTN::OTNObject>` or `std::vector<YourType>`. Every element contains one row.

A reference to a row that does not exist, or a cycle of references (`A` references `B`, `B` references `A`), is a read error.
