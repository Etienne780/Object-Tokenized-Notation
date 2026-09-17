---
id: node_mu55p95x18c549nhu7cu9
name: Writer Internals
---

# Writer Internals

`OTNWriter` collects `OTNObject` instances and serializes them to a `.otn` text file in a two-phase process: first all objects are converted to an internal `SerializedObject` representation, then the file is written in a single forward pass.

## Phase 1 — CreateWriteData

`CreateWriteData` iterates all `OTNObject` instances and calls `AddObject` for each one. `AddObject` resolves nested `OBJECT` references: any column value of type `OBJECT` is recursively added as its own object, and the reference is replaced with an `OBJECT_REF` (integer index) pointing into that child object's row list. This depth-first traversal ensures child objects are always written before the parent.

`ConvertToSerValue` performs the actual OBJECT→OBJECT_REF replacement and also strips list wrappers from OBJECT lists down to a list of integer refs.

If `UseDeduplicateRows(true)` is set, `SerializedObject::AddOrGetRow` checks a row hash map before inserting. Identical rows (same column types and equal values) share a single stored row, and the returned index is reused.

## Phase 2 — WriteBody / WriteObjects

`WriteObjects` emits one OTN object block per `SerializedObject`. Each block has the form:

```otn
ObjectName[rowcount] {
    type/name, type/name, type/name
};
value, value, value; // row 1
value, value, value; // row 2
...                  // row n
```

Column types are written as slash-separated type tokens in the column header line, followed by comma-separated column names. Each row is written as a semicolon-terminated value list.

If `UseDefType` or `UseDefName` is active, frequently used type or name strings are replaced with their pre-assigned numeric ID (`@defType`/`@defName` header tables), reducing file size.

`BufferedIndentedStream` accumulates output in a 64 KB in-memory buffer and flushes to the `std::ofstream` in chunks, avoiding one system call per value.

If `UseOptimizations` is active, all spaces, tabs, and newlines are suppressed.

## OTNStreamWriter

`OTNStreamWriter` writes one row at a time without buffering the full data set.

**BeginObject / WriteRow / EndObject flow:**

1. `BeginObject` writes the object header line with a placeholder row count field (9-character wide). The stream position of this placeholder is saved in `m_countPos`.
2. `WriteRow` converts each value via `OTNDataType<T>` and calls `WriteRowInternal`, which serializes one row to the file immediately.
3. `EndObject` seeks back to `m_countPos` via `seekp`, overwrites the placeholder with the actual row count, then seeks back to the end to resume writing.

**AppendObject flow:**

`AppendObject` calls `EnsureSubObjectsWritten` recursively for any nested `OBJECT` values in the source `OTNObject`. Sub-objects are written before the parent (depth-first). Pointer-based deduplication (`m_objPtrToIndex`) ensures that the same `OTNObject` instance is written only once even if referenced from multiple rows.

`WriteObjectHeaderKnown` is used by `AppendObject` since the row count is already known before writing starts, so no `seekp` is needed.
