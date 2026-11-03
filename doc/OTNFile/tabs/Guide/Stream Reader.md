---
id: node_c5n8r2v6y0dkf3wq7jtm
name: Stream Reader
---

# Stream Reader

`OTNStreamReader` reads large files without loading them into memory. Rows can be read one after another or directly by object name and row index. For small files `OTNReader` is simpler, see [[reading-data]].

## Opening a file

```cpp
OTN::OTNStreamReader reader;
if (!reader.Open("world.otn")) {
    std::cerr << reader.GetError() << '\n';
    return;
}

for (const std::string& name : reader.GetObjectNames())
    std::cout << name << '\n';
```

`Open` scans the file once and builds a small index: the objects with their columns and row counts, and the file position of every 1024th row. The rows themselves are not kept in memory.

## Reading row by row

```cpp
while (reader.NextObject()) {
    std::cout << reader.GetCurrentObjectName() << ": "
              << reader.GetRemainingRowCount() << " rows\n";

    OTN::OTNRow row;
    while (reader.ReadRow(row)) {
        int id = std::get<int>(row[0].value);
        // ...
    }
}
```

- `NextObject` moves to the next object. Rows that were not read are skipped. `SkipCurrentObject` skips them explicitly.
- `ReadRow` returns the values of the next row as `OTNValue`s. The type of a value is in `row[i].type`, the value is read with `std::get`.
- `GetColumnNames()`, `GetColumnTypes()`, `GetRemainingRowCount()`, `HasMoreRows()` and `HasMoreObjects()` describe the current position.

## Random access

Any row or object can be read directly, independent of the current position:

```cpp
std::optional<OTN::OTNObject> row = reader.ReadObjectRow("Enemy", 5000);
if (row) {
    std::string name = row->GetValue<std::string>(0, "name", "");
    auto pos = row->TryGetValue<Vector2>(0, "pos");
}

std::optional<OTN::OTNObject> config = reader.ReadObject("Config");
```

- `ReadObjectRow` returns an `OTNObject` with exactly this one row. Values are read with `GetValue` / `TryGetValue` like after `OTNReader`.
- `ReadObject` loads a complete object into memory, so only use it for small objects.
- Both return `std::nullopt` if the object or row does not exist.

To find a row, the reader jumps to the closest stored position and reads at most 1023 rows from there.

## Object references

References are resolved by default: a `Ref<Vector2>` value is loaded as an `OTNObject` with the referenced row (references inside it are resolved as well). Therefore custom types can be read from `ReadObjectRow` results with `TryGetValue<Vector2>`.

Every resolved reference reads its row from the file. If you only need the indices, turn it off:

```cpp
reader.SetResolveObjectRefs(false);
```

Reference values then have the type `OTNBaseType::OBJECT_REF` and contain the name of the referenced object and the row index.

A reference to a row that does not exist and cyclic references are errors.

## Changed files

Before every read, the size and modification time of the file are checked. If the file was changed (for example written again by an `OTNStreamWriter`), it is scanned again and the reader continues at the same object and row. If the current object no longer exists, the reader reports an error.

## Errors

On an error (invalid file, missing object in a reference, ...) the reader becomes invalid and all further reads return `false` / `std::nullopt`. `GetError()` returns the reason with the line and column in the file. See [[error-handling]].
