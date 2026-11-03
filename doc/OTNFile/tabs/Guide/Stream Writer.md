---
id: node_q7k2m9x4w1btr8vz3hpd
name: Stream Writer
---

# Stream Writer

`OTNStreamWriter` writes data row by row. Rows are not kept in memory, so the amount of data is only limited by the disk. Use it when the data does not fit into memory or is produced step by step. Otherwise `OTNWriter` is simpler, see [[writing-data]].

## Writing rows

```cpp
OTN::OTNStreamWriter writer;
writer.Open("enemies.otn");

writer.BeginObject("Enemy",
    OTN::col("name", "String"),
    OTN::col("hp",   "int"),
    OTN::col("pos",  "Vector2"),
    OTN::col("path", "Vector2[]"));

for (const Enemy& e : enemies)
    writer.WriteRow(e.name, e.hp, e.pos, e.path);

writer.EndObject();

if (!writer.Close())
    std::cerr << writer.GetError() << '\n';
```

- `BeginObject` defines the columns. The types use the same strings as `SetTypes`, see [[types]]. Instead of `OTN::col(...)` two vectors with names and types can be passed.
- `WriteRow` takes one value per column. `WriteRowList(row)` takes a prepared `OTNRow`.
- Custom types ([[custom-types]]) and `OTNObject`s can be written directly. They are stored as rows of their own object, the column stores the row index.
- `EndObject` finishes the object. `Close` writes the file.

## Writing complete objects

`AppendObject` writes an `OTNObject` including all nested objects:

```cpp
writer.AppendObject(players);
```

`BeginObject` / `WriteRow` and `AppendObject` can be mixed freely, but `AppendObject` cannot be called while an object is open.

## The file is written on Close

All rows are first collected in a temporary file next to the target (`enemies.otn.tmp`). `Close()` builds the real file from it and deletes the temporary file. The destructor calls `Close()` as well. An example of the temporary file and how the final file is built from it is in [[writer-internals]].

Until `Close()`:

- the target file is empty,
- disk space for the data is needed twice (temporary file and target file) while `Close()` copies the rows,
- if the program crashes, only the temporary file contains the data.

## Same object name multiple times

Rows can be added to an object at any time. `BeginObject` with a name that already has rows, `AppendObject` with an object of the same name or a custom type used in several places all add rows to the same object:

```cpp
writer.BeginObject("Vector2", OTN::col("x", "float"), OTN::col("y", "float"));
writer.WriteRow(9.0f, 10.0f);  // added to the Vector2 rows written by WriteRow above
writer.EndObject();
```

The column names and types must be the same every time, otherwise it is an error.

## References by index

Instead of a value, a reference column can also get the row index of an existing row. `GetWrittenRowCount(name)` returns the number of rows of an object written so far, which is the index of the next row:

```cpp
size_t statsIndex = writer.GetWrittenRowCount("WeaponStats");

writer.BeginObject("WeaponStats", OTN::col("damage", "float"), OTN::col("speed", "float"));
writer.WriteRow(15.5f, 1.2f);
writer.EndObject();

writer.BeginObject("Weapon", OTN::col("name", "String"), OTN::col("stats", "WeaponStats"));
writer.WriteRow(std::string("Sword"), statsIndex);
writer.EndObject();
```

## Duplicates

The same object instance is written only once: an `OTNObject` that is referenced from several rows gets one set of rows, all references point to it. Values (like two equal `Vector2`) are written as separate rows. There is no `UseDeduplicateRows` option.

## Limits

- `UseDefType`, `UseDefName` and `UseOptimizations` are not available.
- An object that contains itself (directly or through other objects) is an error.
- After an error the writer stays invalid. `Close()` still writes all rows that were written successfully.
- `Open()` on an open writer closes the current file first.

Files of `OTNStreamWriter` can be read with `OTNReader` and `OTNStreamReader`.
