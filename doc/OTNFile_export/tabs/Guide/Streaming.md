---
id: node_mu55p95x1u9dp3uu56hz2
name: Streaming
---

# Streaming

For large datasets where loading the full file into memory is not acceptable, OTN provides streaming variants of both the reader and writer. These keep only one object's worth of data in memory at a time.

> **Note:** The streaming classes have fewer features than their non-streaming counterparts. Object references are unresolved in `OTNStreamReader`, and `OTNStreamWriter` does not apply DefType/DefName optimizations. If your data fits comfortably in memory, prefer `OTNReader` and `OTNWriter` for the full feature set.

## OTNStreamReader

Reads objects and rows one at a time without loading the whole file.

```cpp
OTN::OTNStreamReader reader;
if (!reader.Open("data.otn")) {
    std::cerr << reader.GetError() << std::endl;
    return;
}

while (reader.NextObject()) {
    std::cout << reader.GetCurrentObjectName() << "\n";

    OTN::OTNRow row;
    while (reader.ReadRow(row)) {
        // process individual row values
        // row[0].value, row[1].value, etc.
    }
}
```

Objects must be read **sequentially** — there is no random access. If you call `NextObject()` before consuming all rows of the current object, the remaining rows are skipped automatically. You can also skip explicitly:

```cpp
while (reader.NextObject()) {
    if (reader.GetCurrentObjectName() != "Items") {
        reader.SkipCurrentObject();
        continue;
    }
    // process Items...
}
```

**Available metadata for the current object:**

```cpp
reader.GetCurrentObjectName();  // object name
reader.GetColumnNames();        // column names
reader.GetColumnTypes();        // type descriptors
reader.GetRemainingRowCount();  // rows not yet read
reader.HasMoreRows();           // true while rows remain
reader.HasMoreObjects();        // true while objects remain
```

**Limitations of OTNStreamReader:**

- Object references (`object_ref` columns) are returned as unresolved `OTNObjectRef` values. They are not resolved to actual `OTNObject` instances.
- No random access to rows or objects.
- Objects must be read in file order.

## OTNStreamWriter

Writes objects and rows one at a time to a file without buffering all data in memory.

### Streaming workflow (BeginObject / WriteRow / EndObject)

Use this when you are generating rows on the fly and the total count is not known upfront.

```cpp
OTN::OTNStreamWriter writer;
writer.Open("output.otn");

writer.BeginObject("Items",
    OTN::col("id",    "int"),
    OTN::col("name",  "String"),
    OTN::col("value", "float"));

for (auto& item : myItems) {
    writer.WriteRow(item.id, item.name, item.value);
}

writer.EndObject();
writer.Close();
```

The row count is not written into the file header until `EndObject()` is called. The writer patches the count back in via `seekp` at that point.

### High-level AppendObject

For complete `OTNObject` instances (including nested custom types), use `AppendObject`. It handles sub-object serialization depth-first and deduplicates repeated sub-object instances automatically.

```cpp
writer.AppendObject(myComplexObject);
```

### Object references in streaming

When you need one object to reference rows of another, use integer indices directly. `GetWrittenRowCount()` gives you the next available index for a named object.

```cpp
size_t statsIdx = writer.GetWrittenRowCount("WeaponStats");

writer.BeginObject("WeaponStats",
    OTN::col("damage", "float"),
    OTN::col("speed",  "float"));
writer.WriteRow(15.5f, 1.2f);
writer.EndObject();

writer.BeginObject("Weapon",
    OTN::col("name",  "String"),
    OTN::col("stats", "WeaponStats"));
writer.WriteRow("Sword", statsIdx); // reference by row index
writer.EndObject();

writer.Close();
```

Files written by `OTNStreamWriter` are fully compatible with both `OTNReader` and `OTNStreamReader`.
