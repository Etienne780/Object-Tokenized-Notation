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
| `CharSource` | Characters from memory (`OpenMemory`) or from a file with a 64 KB buffer (`OpenFile`). Can jump to a `FilePosition` (`Seek`). |
| `Tokenizer` | Creates tokens on demand from a `CharSource`: `Next`, `Peek`, `NextIf`, `Expect`. `Tell` / `Seek` save and restore the position. |

All readers use the same tokenizer. `OTNReader` reads from memory, `OTNStreamReader` from the file. Errors contain the line and column of the token.

Token types: `KEYWORD_PREFIX` (`@`), `IDENTIFIER`, `NUMBER`, `STRING`, `MINUS`, `COLON`, `EQUALS`, `COMMA`, `SLASH`, `SEMICOLON`, `BLOCK_BEGIN` / `BLOCK_END`, `LIST_BEGIN` / `LIST_END`, `REF_BEGIN` / `REF_END`, `END_OF_FILE`.

## Versions

Every format version has its own namespace with read and write functions:

```cpp
namespace Detail {
    // independent of the version
    std::optional<uint8_t> ReadVersion(Tokenizer& tok);

    namespace V1 {
        // read
        std::optional<FileHeaderInfo>   ReadFileHeader(Tokenizer&, uint8_t version);
        bool                            HasMoreObjects(Tokenizer&);
        std::optional<ObjectHeaderInfo> ReadObjectHeader(Tokenizer&, const FileHeaderInfo&);
        std::optional<OTNRow>           ReadRow(Tokenizer&, const std::vector<OTNTypeDesc>&);
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

    namespace Current = V1; // version that is written
    const ReadSyntax* GetReadSyntax(uint8_t version);
}
```

- **Writing** always uses `Current`. A `static_assert` checks that `Current` matches `OTN::VERSION`.
- **Reading** first calls `ReadVersion`, then `GetReadSyntax(version)` returns the table of function pointers (`ReadSyntax`) for that version. Old files can therefore still be read after the format changed.

## Adding a version

1. Add `namespace V2` **after** `V1`. Functions that did not change are reused from `V1` (`using V1::ReadRow;` or by calling them).
2. Create `V2::READ_SYNTAX` with the V2 functions where something changed and the V1 functions otherwise.
3. Add `case 2` to `GetReadSyntax`.
4. Set `OTN::VERSION = 2` and `namespace Current = V2`, and update the `static_assert`.
5. If V2 needs new information, add fields to the info structs. Older versions leave them at their default.

The classes do not have to be changed as long as the info structs contain everything they need.

## File grammar of V1

```text
file        = "@version" ":" NUMBER ";" header* "@object" ":" "{" object* "}" ";" EOF
header      = ("@defType" | "@defName") ":" definition ("," definition)* ";"
definition  = IDENTIFIER "=" NUMBER
object      = IDENTIFIER "[" NUMBER "]" "{" (column ("," column)*)? "}" ";" row*
column      = type "/" name
type        = (IDENTIFIER | "Ref" "<" IDENTIFIER ">" | NUMBER) ("[" "]")*
name        = IDENTIFIER | NUMBER
row         = value ("," value)* ";"
value       = NUMBER | "-" NUMBER | STRING | "true" | "false" | list
list        = "[" (value ("," value)*)? "]"
```

The number of rows is given in the object header. A `NUMBER` as type or name is looked up in the `@defType` / `@defName` table.
