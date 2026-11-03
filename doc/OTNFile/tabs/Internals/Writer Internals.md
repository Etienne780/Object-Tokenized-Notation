---
id: node_mu55p95x18c549nhu7cu9
name: Writer Internals
---

# Writer Internals

Both writers turn `OTNObject`s into rows with row indices instead of nested objects and pass them to the syntax functions of `Detail::Current` (see [[syntax-layer]]).

## OTNWriter

`Save()` / `SaveToString()` run three steps.

**1. CreateWriteData**

Every appended object is converted into a `SerializedObject` (column names, column types, rows). `AddObject` goes through all values:

- An `OBJECT` value is added to the `SerializedObject` of its name (recursively) and replaced by the row index it got. A normal `OTNObject` gives one index per row, so the column becomes a list (`Ref<X>[]`).
- Lists are converted element by element.
- With `UseDeduplicateRows`, `AddOrGetRow` looks up a hash of the row first. An identical row returns its existing index.

Objects with the same name end up in the same `SerializedObject`. Afterwards the type usage is counted for `@defType` and the column name usage for `@defName`. A type or name gets a number if it is used more than once and `length * uses > 9`.

**2. WriteHeader**

`Current::WriteFileHeader` writes `@version` and the `@defType` / `@defName` tables.

**3. WriteBody**

`WriteObjectBlockBegin`, then for every `SerializedObject` `WriteObjectHeader` and one `WriteRow` per row, then `WriteObjectBlockEnd`.

The text is collected in `WriterData::buffer`. `FlushBuffer` writes it to the file whenever it is larger than 64 KB, so memory use does not grow with the file size. `SaveToString` returns the buffer instead.

## OTNStreamWriter

The stream writer does not know the final row counts while writing, and rows of different objects arrive mixed (a `WriteRow` with a `Vector2` value creates a `Vector2` row in between). The file needs every object as one block with its row count. Therefore:

**Temporary file**

`Open()` creates `<name>.otn.tmp`. Every row is appended to it immediately. For every object the writer keeps an `ObjectBlock` in memory:

```cpp
struct RowRange    { uint64_t offset; uint64_t length; };
struct ObjectBlock { Detail::ObjectHeaderInfo header; std::vector<RowRange> ranges; };
```

`header.rowCount` counts the rows, `ranges` stores where they are in the temporary file. Rows that directly follow the last range of the same object extend it instead of adding a new range.

The temporary file only contains data rows: no `@version`, no object headers and no `@object` block. Every row is already formatted exactly as in the final file (tab, values, `;`, line break), so `Close()` can copy the bytes without changing them.

**Example**

```cpp
OTN::OTNStreamWriter w;
w.Open("world.otn");

w.BeginObject("Position", OTN::col("id", "int"), OTN::col("pos", "Vector2"));
w.WriteRow(1, Vector2{ 1, 2 });
w.WriteRow(2, Vector2{ 3, 4 });
w.EndObject();

w.BeginObject("Vector2", OTN::col("x", "float"), OTN::col("y", "float"));
w.WriteRow(5.0f, 6.0f);
w.EndObject();

w.Close();
```

Step by step:

{hover=row}
| Call | Appended to `world.otn.tmp` | Index in memory |
|---|---|---|
| `Open` | - (file created empty, `world.otn` created empty) | - |
| `BeginObject("Position")` | - | `Position`: 0 rows |
| `WriteRow(1, Vector2{1, 2})` | 1. the `Vector2` row `1, 2;` at offset 0 | `Vector2`: 1 row, ranges `[0, 7]` |
| | 2. the `Position` row `1, 0;` at offset 7 (`0` = `Vector2` row 0) | `Position`: 1 row, ranges `[7, 7]` |
| `WriteRow(2, Vector2{3, 4})` | 1. `3, 4;` at offset 14 | `Vector2`: 2 rows, ranges `[0, 7] [14, 7]` |
| | 2. `2, 1;` at offset 21 | `Position`: 2 rows, ranges `[7, 7] [21, 7]` |
| `EndObject` | - | - |
| `BeginObject("Vector2")` | - (name exists, columns match) | - |
| `WriteRow(5.0f, 6.0f)` | `5, 6;` at offset 28 | `Vector2`: 3 rows, ranges `[0, 7] [14, 7] [28, 7]` |
| `Close` | see below | |

A range is `[offset, length]` in bytes. Every row here is 7 bytes long: tab, `1, 2;` and the line break. A further `WriteRow(7.0f, 8.0f)` would directly follow the last `Vector2` range, so the range would grow to `[28, 14]` instead of adding a new one. A sub-object is always written before the row that references it, because the row needs its index.

Content of `world.otn.tmp` before `Close()` (`→` is a tab):

```text
offset  bytes        object
0       →1, 2;\n     Vector2  row 0
7       →1, 0;\n     Position row 0
14      →3, 4;\n     Vector2  row 1
21      →2, 1;\n     Position row 1
28      →5, 6;\n     Vector2  row 2
```

The rows of the two objects are mixed. Only the ranges in memory tell which bytes belong to which object. `std::ofstream` buffers the writes, so the file on disk can lag behind until it is closed.

**Close**

`WriteFile` closes the temporary file and builds `world.otn`:

1. `WriteFileHeader`: `@version: 1;` and the start of `@object`.
2. For every block in the order of first use (`Position`, then `Vector2`): `WriteObjectHeader` with the final row count, then copy the ranges of the block from the temporary file (64 KB buffer).
3. `WriteObjectBlockEnd`, then the temporary file is deleted.

```otn
@version: 1;

@object: {
	Position[2] {
		int/id, Ref<Vector2>/pos
	};
	1, 0;          ← copied from offset 7
	2, 1;          ← copied from offset 21
	Vector2[3] {
		float/x, float/y
	};
	1, 2;          ← copied from offset 0
	3, 4;          ← copied from offset 14
	5, 6;          ← copied from offset 28
};
```

(The arrows are only explanation, they are not in the file.)

Until `Close()` the target file `world.otn` stays empty.

**Sub-objects**

`ConvertValue` replaces `OBJECT` values by row indices. `WriteSubObject` writes the sub-object first:

- `m_objPtrToIndex` maps an `OTNObject*` to its first row index. A `weak_ptr` detects if the address was reused by a new object. A known object is not written again.
- `WriteObjectRows` converts all rows of the object (which writes its own sub-objects) and appends the finished rows in one piece, so the rows of one object get consecutive indices.
- `m_objectsInProgress` contains the objects that are being converted. Finding one of them again means the object contains itself, which is an error.

**Blocks with the same name**

`GetBlock` returns the block of a name or creates it. An existing block must have the same column names and types. A deduced type of an empty list (`UNKNOWN[]`) is accepted for every list type with at least that depth. New blocks are checked with `WriteObjectHeader` right away, so invalid column types are reported before any row is written.

**Memory**

Memory use is the index (one `RowRange` per range) plus the object that is currently converted. If two objects alternate row by row, every row gets its own range (16 bytes).
