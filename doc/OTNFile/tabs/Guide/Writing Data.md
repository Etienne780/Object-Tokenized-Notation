---
id: node_mu55p95xweb2mx16dzzse
name: Writing Data
---

# Writing Data

Build your data with `OTNObject`, then save it with `OTNWriter`. `OTNWriter` keeps all objects in memory until `ClearObjects()` is called. For data that does not fit into memory use the [[stream-writer]].

## Building an object

```cpp
OTN::OTNObject items{ "Items" };
items.SetNames("id", "name", "value")
     .AddDataRow(1, "Sword",  42.5f)
     .AddDataRow(2, "Shield", 17.25f)
     .AddDataRow(3, "Bow",     8.0f);
```

- `SetNames` defines the columns. Names must be unique and may contain letters, digits, `_` and UTF-8 characters, but must not start with a digit (see [[types]]).
- Every `AddDataRow` adds one row. The number of values must match the number of columns.
- `AddDataRowList(row)` adds a prepared `OTNRow`, `ReserveDataRows(n)` reserves memory for `n` rows.

## Column types

Without `SetTypes()` the column types are deduced from the **first row**. Every following row must use the same types. If the first row contains an empty list, the writer takes the element type from the next row that has one.

To set the types yourself, call `SetTypes()` before the first `AddDataRow()`:

```cpp
items.SetNames("id", "name", "value")
     .SetTypes("int", "String", "double")
     .AddDataRow(1, "Sword", 42.5);
```

Use `"-"`, `"_"` or `""` for a column whose type should still be deduced from the first row. All type strings are listed in [[types]].

Explicit types are useful when:

- all rows contain empty lists in a column (its element type cannot be deduced),
- a column should accept different value types (`any`),
- an object has no rows but should still be written with its columns.

## Saving

```cpp
OTN::OTNWriter writer;
writer.AppendObject(items)
      .AppendObject(enemies)
      .Save("gamedata.otn");
```

- `AppendObject` copies the object, later changes to `items` are not written.
- Every object name may only be used once per file.
- Missing folders are created. A path without extension gets `.otn`, any other extension is an error.

To get the text instead of a file:

```cpp
std::string text;
if (writer.SaveToString(text)) {
    // text contains the complete file
}
```

`ClearObjects()` removes all appended objects, so the writer can be reused for another file. `Reset()` also removes the error state, for example after a failed `Save()`. The options stay in both cases.

## Nested objects

Values can be other `OTNObject`s or your own types ([[custom-types]]). They are written as their own objects and referenced by row index:

```cpp
OTN::OTNObject inventory{ "Item" };
inventory.SetNames("name", "qty")
         .AddDataRow("Sword", 1)
         .AddDataRow("Potion", 5);

OTN::OTNObject player{ "Player" };
player.SetNames("name", "pos", "inventory")
      .AddDataRow("Hero", Vector2{ 1, 2 }, inventory);
```

```otn
@version: 2;

@object: {
	Item[2] {
		String/name, int/qty
	};
	"Sword", 1;
	"Potion", 5;
	Vector2[1] {
		float/x, float/y
	};
	1, 2;
	Player[1] {
		String/name, Ref<Vector2>/pos, Ref<Item>[]/inventory
	};
	"Hero", 0, [0, 1];
};
```

An `OTNObject` with several rows is stored as a **list** of references (`Ref<Item>[]`), so it has to be read back as a list. See [[reading-data]].

## Writer options

All options are set before `Save()` and can be chained.

{hover=row; col 0: width=160}
| Method | Default | Effect |
|---|---|---|
| `UseDefType(true)` | off | Often used type names are replaced by numbers to save storage (`@defType`) |
| `UseDefName(true)` | off | Often used column names are replaced by numbers to save storage (`@defName`) |
| `UseOptimizations(true)` | off | No spaces, line breaks and indentation to save storage if readability is not needed |
| `UseDeduplicateRows(true)` | off | Identical rows of an object are written once and referenced multiple times to save storage |

```cpp
writer.UseDefName(true)
      .UseDefType(true)
      .UseOptimizations(true)
      .AppendObject(items)
      .Save("data.otn");
```

A type or name is only replaced if the saved characters are more than its entry in `@defType` / `@defName` costs, so these options never make the file longer. Small files often get no numbers at all. The output of all options is shown in [[file-format]].

With deduplication, identical rows are stored once per object. Two `Vector2{ 1, 2 }` values in different rows become one `Vector2` row that both rows reference. This also applies to the rows of the objects you append yourself: identical rows there are only written once.

## Errors

`Save()` and `SaveToString()` return `false` on errors, the reason is returned by `GetError()`:

```cpp
if (!writer.Save("data.otn"))
    std::cerr << writer.GetError() << '\n';
```

An `OTNObject` with an error (for example a row with the wrong number of values) is not written, `Save()` fails and `GetError()` names the object. Use `Reset()` to write another file with the same writer. See [[error-handling]].

Before the file is opened, every value is checked against its column, so a written file can always be read again. `Save()` fails (and an existing file is not changed) if:

- a value does not match its column type, for example a string in an `int` column or a number that does not fit,
- a float or double value is NaN or infinite (the format has no literal for them),
- two different objects use the same name, for example an appended object and a custom type with other columns.
