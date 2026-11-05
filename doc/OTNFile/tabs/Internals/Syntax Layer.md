---
id: node_g8d1z5k3n7vxe2qb6ycu
name: Syntax Layer
---

# Syntax Layer

All knowledge about the file syntax is in one place: the region `Syntax` at the top of `OTNFile.cpp`. The four reader and writer classes never write or parse text themselves, they only exchange info structs with the syntax functions. A new format version therefore only needs new syntax functions.

## Info structs

Declared in `OTNFile.h` in `OTN::Detail`, because the classes store them as members:

| Struct | Content |
|---|---|
| `FileHeaderInfo` | Version, `defType` and `defName` tables |
| `ObjectHeaderInfo` | Object name, row count, column names and column types |
| `DefTable` | Numbers to names and back (`@defType` / `@defName`), `GetEntries()` sorted by number |
| `WriteOptions` | `compact`: no spaces, line breaks and indentation |
| `FilePosition` | Offset, line and column in the file |

Write functions **take** an info struct and append text to a `std::string`. Read functions **return** an info struct (`std::optional`, empty on error).

## Reading: CharSource and Tokenizer

| Class | Task |
|---|---|
| `CharSource` | Characters from memory (`OpenMemory`) or from a file in 64 KB blocks, the last 4 used blocks are kept (`OpenFile`). `Available` / `Current` / `Skip` give direct access to the loaded chars. Can jump to a `FilePosition` (`Seek`). |
| `Tokenizer` | Creates tokens on demand from a `CharSource`: `Next`, `Peek`, `NextIf`, `Expect`. `Tell` / `Seek` save and restore the position. The text of a token is a `string_view` into the loaded chars, it is only copied for strings with escape sequences and tokens over a block border. |

All readers use the same tokenizer. `OTNReader` reads from memory, `OTNStreamReader` from the file. Errors contain the line and column of the token.

`ReadVersion` first calls `SkipByteOrderMark`: a UTF-8 BOM is skipped (not counted as column), a file starting with `FF`, `FE` or `00` is rejected as UTF-16 / UTF-32.

An `IDENTIFIER` starts with a letter, `_` or a byte from `0x80` and continues with these and digits (`IsNameStartChar` / `IsNameChar`). `OTNObject::IsNameValid` uses the same functions, so the writers can only produce names the tokenizer can read.

Token types: `KEYWORD_PREFIX` (`@`), `IDENTIFIER`, `NUMBER`, `STRING`, `MINUS`, `COLON`, `EQUALS`, `COMMA`, `SLASH`, `SEMICOLON`, `BLOCK_BEGIN` / `BLOCK_END`, `LIST_BEGIN` / `LIST_END`, `REF_BEGIN` / `REF_END`, `REF_INDEX` (`#`, since version 2), `END_OF_FILE`.

## Versions

Every format version has its own namespace with read and write functions. A newer version is defined after the older one and reuses everything that did not change:

```cpp
namespace Detail {
    // independent of the version
    std::optional<uint8_t> ReadVersion(Tokenizer& tok);

    namespace V1 {
        // read
        std::optional<FileHeaderInfo>   ReadFileHeader(Tokenizer&, uint8_t version);
        bool                            HasMoreObjects(Tokenizer&);
        std::optional<ObjectHeaderInfo> ReadObjectHeader(Tokenizer&, const FileHeaderInfo&);
        std::optional<OTNRow>           ReadRow(Tokenizer&, const std::vector<OTNTypeDesc>&, const FileHeaderInfo&);
        bool                            SkipRow(Tokenizer&);
        bool                            ReadObjectBlockEnd(Tokenizer&);

        // write
        void WriteFileHeader(std::string&, const WriteOptions&, const FileHeaderInfo&);
        void WriteObjectBlockBegin(std::string&, const WriteOptions&);
        void WriteObjectBlockEnd(std::string&, const WriteOptions&);
        bool WriteObjectHeader(std::string&, const WriteOptions&, const ObjectHeaderInfo&,
                               const FileHeaderInfo&, std::string& error);
        bool WriteRow(std::string&, const WriteOptions&, const OTNRow&, std::string& error);

        const ReadSyntax READ_SYNTAX = { ReadFileHeader, HasMoreObjects, ... };
    }

    namespace V2 {
        // new: references in 'any' values, object names in @defType
        std::optional<OTNValue>       ReadAnyValue(Tokenizer&, const V1::ValueContext&);
        std::optional<OTNRow>         ReadRow(Tokenizer&, const std::vector<OTNTypeDesc>&, const FileHeaderInfo&);
        std::optional<FileHeaderInfo> ReadFileHeader(Tokenizer&, uint8_t version);
        bool WriteRow(std::string&, const WriteOptions&, const OTNRow&, const FileHeaderInfo&, std::string& error);

        // unchanged, taken from V1
        using V1::WriteFileHeader;
        using V1::WriteObjectBlockBegin;
        using V1::WriteObjectBlockEnd;
        using V1::WriteObjectHeader;

        const ReadSyntax READ_SYNTAX = { ReadFileHeader, &V1::HasMoreObjects, &V1::ReadObjectHeader,
                                         ReadRow, &V1::SkipRow, &V1::ReadObjectBlockEnd };
    }

    namespace Current = V2; // version that is written
    const ReadSyntax* GetReadSyntax(uint8_t version); // 1 -> V1::READ_SYNTAX, 2 -> V2::READ_SYNTAX
}
```

- **Writing** always uses `Current`. A `static_assert` checks that `Current` matches `OTN::VERSION`.
- **Reading** first calls `ReadVersion`, then `GetReadSyntax(version)` returns the table of function pointers (`ReadSyntax`) for that version. Old files can therefore still be read after the format changed.

## How V2 reuses V1

Reading a value goes through `V1::ReadValue` → `V1::ReadList` → `ReadAnyValue`. Only the reading of `any` values changed in V2, so the value functions of V1 take a `V1::ValueContext`:

```cpp
struct ValueContext {
    const FileHeaderInfo& file;  // for @defType ids
    ReadAnyFunc readAny;         // V1::ReadAnyValue or V2::ReadAnyValue
};
```

`V1::ReadRow` passes `V1::ReadAnyValue`, `V2::ReadRow` passes `V2::ReadAnyValue`. Everything else (lists, primitives, `Ref` columns) is the same code.

`V2::ReadAnyValue` handles `Name#index` and `id#index` and passes all other values to `V1::ReadAnyValue`. `V2::ReadFileHeader` calls `V1::ReadFileHeaderImpl` with `allowObjectTypes = true`, so `@defType` can contain object names.

When writing, `V2::WriteValue` writes `OBJECT_REF` values (`Name#index`, the name replaced by its `@defType` id if there is one) and passes all other values to `V1::WriteValue`. The writers create `OBJECT_REF` values only for objects in `any` columns, in `Ref` columns they stay plain row indices.

`V1::WriteColumnType` writes the `@defType` id of an object name if the table contains it. In version 1 files the table never contains object names, so V1 output does not change.

## Adding a version

1. Add `namespace V3` **after** `V2`. Functions that did not change are reused (`using V2::WriteObjectHeader;` or by calling them).
2. If a function deep in a call chain changes, pass it in like `ValueContext::readAny` instead of copying the whole chain.
3. Create `V3::READ_SYNTAX` with the new functions where something changed and the old ones otherwise.
4. Add `case 3` to `GetReadSyntax`.
5. Set `OTN::VERSION = 3` and `namespace Current = V3`, and update the `static_assert`.
6. If V3 needs new information, add fields to the info structs. Older versions leave them at their default.

The classes do not have to be changed as long as the info structs contain everything they need.

## File grammar of V2

```text
file        = "@version" ":" NUMBER ";" header* "@object" ":" "{" object* "}" ";" EOF
header      = ("@defType" | "@defName") ":" definition ("," definition)* ";"
definition  = IDENTIFIER "=" NUMBER
object      = IDENTIFIER "[" NUMBER "]" "{" (column ("," column)*)? "}" ";" row*
column      = type "/" name
type        = (IDENTIFIER | "Ref" "<" IDENTIFIER ">" | NUMBER) ("[" "]")*
name        = IDENTIFIER | NUMBER
row         = value ("," value)* ";"
value       = NUMBER | "-" NUMBER | STRING | "true" | "false" | reference | list
reference   = (IDENTIFIER | NUMBER) "#" NUMBER          (only in 'any' columns)
list        = "[" (value ("," value)*)? "]"
```

The number of rows is given in the object header. A `NUMBER` as type or name is looked up in the `@defType` / `@defName` table. In `Ref<Name>` columns a value is only the row index (`NUMBER`).

Version 1 is the same without `reference`, and `@defType` may only contain the base types.
