---
id: node_mu55p95x1mur4st1tuzfby
name: File Format
---

# File Format

An `.otn` file is UTF-8 text. It starts with a header of directives, followed by one `@object` block that contains all objects.

```otn
@version: 1;
@defType: float = 0, String = 1;
@defName: value = 0;

@object: {
	Items[2] {
		int/id, 1/name, 0/0
	};
	1, "Sword", 42.5;
	2, "Shield", 17.25;
	Enemies[1] {
		int/id, 1/name, 0/0
	};
	1, "Goblin", 3.5;
};
```

Spaces, tabs and line breaks between tokens have no meaning. Comments are not supported.

## Structure

| Part | Required | Content |
|---|---|---|
| `@version: N;` | yes | Format version, always the first statement. Currently `1`. |
| `@defType: …;` | no | Numbers for type names, see [[deftype-defname]] |
| `@defName: …;` | no | Numbers for column names |
| `@object: { … };` | yes | All objects. Nothing may follow after the closing `};`. |

`@defType` and `@defName` stand between `@version` and `@object`, in any order.

Unknown directives are an error.

## Objects

```otn
Name[rowCount] {
	type/column, type/column, ...
};
value, value, ...;
value, value, ...;
```

- `Name` is the object name. Every name may only appear once per file.
- `rowCount` is the exact number of rows that follow.
- The header lists every column as `type/name`, separated by commas.
- Every row has one value per column, separated by commas and ended by `;`.

Objects can appear in any order. A reference may point to an object that comes later in the file.

## Types

| Type in the file | Values |
|---|---|
| `int`, `int64`, `uint64` | Integers: `42`, `-7` |
| `float`, `double` | Numbers: `1.5`, `-0.25`, `3` |
| `bool` | `true`, `false` |
| `String` | `"text"` with the escapes `\"`, `\\`, `\n`, `\t` |
| `any` | Number, string, `true`/`false` or a list of these |
| `Ref<Name>` | Row index in the object `Name`: `0`, `5` |
| `T[]`, `T[][]`, … | Lists: `[1, 2, 3]`, `[[1, 2], [3]]`, `[]` |

Integers are checked strictly: `3.5` in an `int` column or a value outside the range of the type is an error.

Numbers in `any` columns get the smallest type that fits: `int`, then `int64`, then `uint64`. Numbers with a decimal point or exponent become `double`. See [[types]].

## References

A `Ref<Name>` column stores row indices of the object `Name`:

```otn
	Vector2[3] {
		float/x, float/y
	};
	10, 20;
	0, 0;
	1, 2;
	Entities[1] {
		int/id, Ref<Vector2>/position, Ref<Vector2>[]/path
	};
	1, 0, [1, 2];
```

`position` is row 0 of `Vector2`, `path` the rows 1 and 2. When reading, an index that does not exist in the target object is an error.

## DefType and DefName {#deftype-and-defname}

With `UseDefType(true)` / `UseDefName(true)` the writer replaces often used type and column names by numbers:

```otn
@defType: float = 0, String = 1;
@defName: value = 0;
```

Every entry is `name = number`. In the object headers the number can stand in place of the type or the column name: `0/0` means `float/value`. A number that is not defined is an error.

## Compact form

With `UseOptimizations(true)` all optional spaces and line breaks are removed:

```otn
@version:1;@object:{Items[2]{int/id,String/name,float/value};1,"Sword",42.5;2,"Shield",17.25;};
```

## File names

The extension is `.otn`. A path without an extension gets `.otn`, a different extension is an error. `OTNStreamWriter` uses a temporary file with the extension `.otn.tmp` next to the target file while writing.
