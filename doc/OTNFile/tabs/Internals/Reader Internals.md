---
id: node_mu55p95x1r773m4107y4xr
name: Reader Internals
---

# Reader Internals

Both readers use the `Tokenizer` and the version dependent `ReadSyntax` table (see [[syntax-layer]]). They differ in what they keep in memory.

## OTNReader

**Reading**

`ReadFile` loads the file into a string, `ReadString` uses the given text. `ReadData` then:

1. reads `@version` with `ReadVersion` and gets the `ReadSyntax` for this version,
2. reads the file header (`@defType`, `@defName`, start of `@object`),
3. reads every object: `readObjectHeader`, then `rowCount` times `readRow`,
4. checks the end of the `@object` block.

Every object becomes an `OTNObject` in `ReaderData::objects`. A second object with the same name is an error.

**Resolving references**

While reading, a `Ref<X>` value is an `OBJECT_REF` value (object name and row index). `ResolveObjectRefs` replaces them by `OBJECT` values after all objects are read, so references to objects later in the file work:

1. Find the object and check the row index.
2. Copy the referenced row and resolve its references first (recursion).
3. Create an `OTNObject` with this one row and the columns of the target.

The `RefStack` contains the rows that are currently being resolved. Reaching one of them again is a cycle and an error.

## OTNStreamReader

**Scan**

`Open` reads the file once from start to end (`CharSource::OpenFile`, 64 KB buffer) and creates the index:

```cpp
struct ObjectInfo {
    Detail::ObjectHeaderInfo header;              // name, row count, columns
    std::vector<Detail::FilePosition> rowCheckpoints; // row 0, 1024, 2048, ...
};
```

Rows are only skipped (`skipRow`), not converted. Only every 1024th row position is stored, so the index stays small for large files.

**Reading rows**

- `NextObject` / `ReadRow` keep a current object and row. `ReadRow` reads the next row at the tokenizer position.
- `ReadObjectRow(name, row)` seeks to checkpoint `row / 1024`, skips up to 1023 rows and reads the row. The sequential position is saved and restored around it.
- `ReadObject` reads all rows of one object into an `OTNObject`.

**Resolving references**

With `SetResolveObjectRefs(true)` (default) every `OBJECT_REF` in a returned row is replaced by an `OTNObject`: `LoadObjectRow` reads the referenced row with the same seek logic and resolves its references recursively. A stack of `RowKey`s (object name and row) detects cycles.

**Changed files**

Before every read, `EnsureUpToDate` compares the file size and modification time with the values from the scan. If they differ, `Scan` runs again. The current object is found by name and the row position is restored. If the object is gone or has fewer rows than already read, the reader reports an error.

**Memory**

The reader keeps the index (one `FilePosition` per 1024 rows and object) and a 64 KB read buffer. Rows are only in memory while they are returned.
