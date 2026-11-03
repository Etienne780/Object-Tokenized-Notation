---
id: node_mu55p95x1mur4st1tuzfby
name: File Format
---

# File Format

An `.otn` file is UTF-8 text. It starts with a header of directives, followed by one `@object` block that contains all objects.

```otn
@version: 2;

@object: {
	Items[2] {
		int/id, String/name, float/value
	};
	1, "Sword", 42.5;
	2, "Shield", 17.25;
	Enemies[1] {
		int/id, String/name, float/value
	};
	1, "Goblin", 3.5;
};
```

Spaces, tabs and line breaks between tokens have no meaning. Comments are not supported. A UTF-8 byte order mark at the start is allowed, UTF-16 and UTF-32 files are not (see [[types]]).

## Structure

| Part | Required | Content |
|---|---|---|
| `@version: N;` | yes | Format version, always the first statement. Files are written with version `2`, versions `1` and `2` can be read (see [[file-versions]]). |
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

- `Name` is the object name. Every name may only appear once per file. Names consist of letters, digits, `_` and UTF-8 characters and do not start with a digit.
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
| `any` | Number, string, `true`/`false`, reference `Name#index` or a list of these |
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

### References in any columns

An `any` column has no object type, so a reference contains the object name: `Name#index`.

```otn
	Values[5] {
		any/value
	};
	42;
	Vector2#0;
	[Vector2#1, Vector2#2];
	[Item#0, Item#1];
	"text";
```

- `Vector2#0` is row 0 of `Vector2`, a single custom type value.
- A list of custom type values is a list of references.
- An `OTNObject` with several rows is a list of references to all its rows (`[Item#0, Item#1]`).

Like in `Ref` columns, a missing object or row is an error when reading.

## DefType and DefName {#deftype-and-defname}

With `UseDefType(true)` / `UseDefName(true)` the writer replaces type and column names by numbers where this makes the file shorter:

```otn
@version: 2;
@defType: String = 0, float = 1;

@object: {
	Quests[1] {
		int/id, 0/name, 1/value
	};
	1, "A", 1.5;
	Npcs[1] {
		int/id, 0/name, 1/value
	};
	1, "A", 1.5;
	Enemies[1] {
		int/id, 0/name, 1/value
	};
	1, "A", 1.5;
	Items[1] {
		int/id, 0/name, 1/value
	};
	1, "A", 1.5;
};
```

Every entry is `name = number`. In the object headers the number can stand in place of the type or the column name: `1/value` means `float/value`, with `@defName` the name can be a number as well (`1/0`). A number that is not defined is an error.

A name only gets a number if the characters saved by all its uses are more than the entry costs (`String = 0, `), and the table as a whole has to pay for the directive (`@defType: ;`) too. Names that save the most get the smallest numbers. In the example `int` (3 characters) and the column names are not replaced, because they would not save enough. So the file is never longer than without these options.

`@defType` can also contain object names. Then the number replaces `Ref<Name>` in the header and the name in references of `any` columns:

```otn
@version: 2;
@defType: Vector2 = 0;

@object: {
	Vector2[12] {
		float/x, float/y
	};
	0, 0;
	0, 0;
	1, 0;
	0, 1;
	...
	Points[6] {
		0/pos, any/extra
	};
	0, 0#1;
	2, 0#3;
	4, 0#5;
	...
};
```

`0/pos` means `Ref<Vector2>/pos`, `0#1` means `Vector2#1`. For object names every reference in an `any` column counts as a use, so they get a number quickly. A number in front of `#` that stands for a type like `float` is an error.

## Compact form

With `UseOptimizations(true)` all optional spaces and line breaks are removed:

```otn
@version:2;@object:{Items[2]{int/id,String/name,float/value};1,"Sword",42.5;2,"Shield",17.25;};
```

## File names

The extension is `.otn`. A path without an extension gets `.otn`, a different extension is an error. `OTNStreamWriter` uses a temporary file with the extension `.otn.tmp` next to the target file while writing.

## Versions {#file-versions}

The writers always write the newest version. The readers read every version, the version is taken from `@version`.

| Version | Changes |
|---|---|
| `1` | First version |
| `2` | References in `any` columns (`Name#index`), object names in `@defType` |

A version 1 file is still valid in version 2. A file that uses version 2 syntax with `@version: 1` is an error.
