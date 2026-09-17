---
id: node_mu55p95xweb2mx16dzzse
name: Writing Data
---

# Writing Data

Use `OTNObject` to build your data in memory, then `OTNWriter` to save it to a file.

## Basic example

```cpp
OTN::OTNObject obj{ "Items" };
obj.SetNames("id", "name", "value")
   .AddDataRow(1, "Sword",  42.5f)
   .AddDataRow(2, "Shield", 17.3f)
   .AddDataRow(3, "Bow",     8.1f);

OTN::OTNWriter writer;
writer.AppendObject(obj)
      .Save("data.otn");
```

## Setting column types explicitly

Types are deduced automatically from the first row you add. If you want to be explicit, call `SetTypes()` before `AddDataRow()`:

```cpp
obj.SetNames("id", "name", "value")
   .SetTypes("int", "String", "float")
   .AddDataRow(1, "Sword", 42.5f);
```

Supported type strings: `int`, `int64`, `uint64`, `float`, `double`, `bool`, `String`, `any`, and any custom object name (e.g. `"WeaponStats"`). Append `[]` for arrays (e.g. `"int[]"`).

Use `"-"`, `""`, or `"_"` as a placeholder to skip defining a type for that column — it will be deduced from the first inserted row.

## Multiple objects in one file

```cpp
OTN::OTNWriter writer;
writer.AppendObject(itemsObj)
      .AppendObject(enemiesObj)
      .AppendObject(configObj)
      .Save("gamedata.otn");
```

## Serializing to a string

```cpp
std::string text;
if (writer.SaveToString(text)) {
    // text now contains the full OTN content
}
```

## Writer options

All options are set before calling `Save()` and support method chaining.

| Option | Method | Default | Description |
|---|---|---|---|
| DefName | `UseDefName(true)` | off | Replaces frequently used column names with numeric IDs to reduce file size |
| DefType | `UseDefType(true)` | off | Replaces frequently used type names with numeric IDs |
| Optimizations | `UseOptimizations(true)` | off | Removes all spaces and line breaks from the output |
| Deduplication | `UseDeduplicateRows(true)` | off | Skips writing identical rows that already exist in the object |

```cpp
OTN::OTNWriter writer;
writer.UseDefName(true)
      .UseDefType(true)
      .UseOptimizations(true)
      .AppendObject(obj)
      .Save("data.otn");
```

## Error handling

```cpp
OTN::OTNWriter writer;
writer.AppendObject(obj);
if (!writer.Save("data.otn")) {
    std::cerr << writer.GetError() << std::endl;
}
```

`OTNObject` itself can also reach an error state. Always check before passing it to the writer:

```cpp
if (!obj.IsValid()) {
    std::cerr << obj.GetError() << std::endl;
}
```
