---
id: node_mu55p95x1mur4st1tuzfby
name: File Format
---

# File Format

A `.otn` file is plain UTF-8 text. It consists of a small header section followed by one or more object blocks.

## Overall structure

```
@version: 1;

@defType: {
    0 = int,
    1 = float,
    2 = String
};

@defName: {
    0 = id,
    1 = name,
    2 = value
};

@object: {

    Items[3] {
        int/String/float,   id,name,value   = 1, "Sword",  42.5;
        int/String/float,   id,name,value   = 2, "Shield", 17.3;
        int/String/float,   id,name,value   = 3, "Bow",    8.1;
    }

    Player[1] {
        int/String/int[],   id,name,tags    = 1, "Hero", [10, 20, 30];
    }

}
```

## Header directives

All directives begin with `@` and are terminated with `;`.

| Directive | Required | Description |
|---|---|---|
| `@version: N;` | yes | Format version number. Currently always `1`. |
| `@defType: { id = typeName, ... };` | no | Maps numeric IDs to type name strings. Emitted by `OTNWriter` when `UseDefType(true)` is set. |
| `@defName: { id = colName, ... };` | no | Maps numeric IDs to column name strings. Emitted when `UseDefName(true)` is set. |
| `@object: { ... };` | yes | Contains all object blocks. |

## Object block syntax

Each object inside `@object: { }` follows this structure:

```
ObjectName[rowCount] {
    type/type/type,   name, name, name   = value, value, value;
    ...
}
```

- `ObjectName` — the identifier used to retrieve the object via `TryGetObject`.
- `[rowCount]` — the number of data rows that follow.
- The **column header line** lists types (slash-separated) and names (comma-separated), separated by an `=`.
- Each **data row** is a comma-separated list of values terminated by `;`.

## Type tokens

| Type string | Maps to |
|---|---|
| `int` | `INT` (32-bit signed) |
| `int64` | `INT64` |
| `uint64` | `UINT64` |
| `float` | `FLOAT` |
| `double` | `DOUBLE` |
| `bool` | `BOOL` (`true` / `false`) |
| `String` | `STRING` (quoted with `"`) |
| `ObjectName` | `OBJECT` (reference to another object, written as integer index) |
| `any` | `ANY` (type stored inline per value) |
| `T[]` | `LIST` of `T` (one bracket pair per nesting level) |

## Value syntax

| Type | Example in file |
|---|---|
| Integer | `42` |
| Negative integer | `-7` |
| Float/double | `3.14` or `-0.5` |
| Bool | `true` or `false` |
| String | `"hello world"` |
| List | `[1, 2, 3]` |
| Nested list | `[[1, 2], [3, 4]]` |
| Object reference | `<ObjectName, 0>` (name + row index, internal form) or just an integer when written via `OTNStreamWriter` |

## DefType / DefName substitution

When `@defType` or `@defName` is present, any numeric ID found in a type position or name position is looked up in the corresponding map and replaced with the actual string before parsing continues. This means the on-disk representation may look like:

```
@defType: { 0 = int, 1 = String, 2 = float };

Items[2] {
    0/1/2,  id, name, value = 1, "Sword", 42.5;
    0/1/2,  id, name, value = 2, "Shield", 17.3;
}
```

## Optimized (minified) output

With `UseOptimizations(true)`, all whitespace and newlines are removed:

```
@version:1;@object:{Items[2]{int/String/float,id,name,value=1,"Sword",42.5;2,"Shield",17.3;}}
```

Readers handle both forms identically since the tokenizer is whitespace-insensitive.

## File extension

The standard extension is `.otn`. It is appended automatically by `OTNWriter`, `OTNReader`, `OTNStreamWriter`, and `OTNStreamReader` if the path does not already end in `.otn`.
