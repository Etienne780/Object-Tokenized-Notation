---
id: node_mu55p95x1r773m4107y4xr
name: Reader & Parser
---

# Reader & Parser

`OTNReader` parses a `.otn` file in three stages: file loading, tokenization, and parsing.

## Stage 1 — File loading

`ReadFile` opens the file with `std::ifstream`, reads the entire content into a `std::string`, and passes it to `ReadData`. `ReadString` skips the file step and feeds the string directly.

## Stage 2 — Tokenization (OTNTokenizer)

`OTNTokenizer` operates on a `string_view` over the in-memory content. It walks the buffer character by character and emits `Token` instances into a flat `std::vector<Token>`.

Each token carries its type, text content, and the line/column position for error messages. The token types map directly to OTN syntax characters and identifiers:

| Token type | Source character(s) |
|---|---|
| `IDENTIFIER` | Letters, digits, `_` sequences |
| `NUMBER` | Digit sequences (integers and decimals) |
| `STRING` | Quoted string literals `"..."` |
| `KEYWORD_PREFIX` | `@` |
| `COLON` | `:` |
| `EQUALS` | `=` |
| `COMMA` | `,` |
| `SLASH` | `/` |
| `SEMICOLON` | `;` |
| `BLOCK_BEGIN` / `BLOCK_END` | `{` / `}` |
| `LIST_BEGIN` / `LIST_END` | `[` / `]` |
| `REF_BEGIN` / `REF_END` | `<` / `>` |
| `MINUS` | `-` (kept separate to handle negative numbers) |
| `END_OF_FILE` | end of input |

Whitespace and comments (if any) are skipped silently. The tokenizer does not allocate sub-strings; `Token::text` is either a copy of the matched characters or left empty for single-character tokens.

## Stage 3 — Parsing (OTNReaderV1)

`OTNReaderV1` consumes the token stream produced by the tokenizer.

**Top-level structure:**

```otn
@version: 1;
@defType: int = 0, float = 1, ...; // ← optional
@defName: value = 0, count = 1, ...; // ← optional

@object: {
	Players[2] {
		0/id, 0/level, 1/0
	};
	0, 30, 0;
	1, 30, 1;
	
	Enemies[2] {
		0/id, 0/1, 1/0
	};
	0, 20, 0;
	1, 20, 1;
	
	Maps[3] {
		0/id, 0/1, 1/0
	};
	0, 20, 0;
	1, 20, 1;
	1, 20, 2;
	
	Levels[2] {
		0/id, 0/1, 1/0
	};
	0, 20, 0;
	1, 20, 1;
};
```

**ParseTopLevel** processes directives in order:
1. `@version` — reads the format version and stores it in `ReaderData`.
2. `@defType` (optional) — builds a numeric-ID-to-type-string map.
3. `@defName` (optional) — builds a numeric-ID-to-name-string map.
4. `@object` block — calls `ParseObject` for each named object inside.

**ParseObject** reads the object name, row count from `[N]`, then calls `ParseHeaderBlock` (column types and names) and `ParseDataRows`.

**ParseDataRows** reads exactly `rowCount` rows. Each row is a semicolon-terminated list of values. Values are converted to `OTNValue` by `TokenToOTNValue`, which dispatches based on the column's `OTNTypeDesc`.

## Reference resolution

After all objects are parsed, `ResolveOTNObjectRefs` walks every object and replaces `OBJECT_REF` values (integer indices) with `OTNObjectPtr` values pointing to the actual `OTNObject` at that index. This is what makes reading back nested object references transparent to the user — by the time `ReadFile` returns, all references are fully resolved and you get real `OTNObject` instances from `GetValue<OTNObject>` or `TryGetValue<OTNObject>`.

If a referenced object name or index does not exist, an error is added and the reader enters an invalid state.

## OTNStreamReader parsing

`OTNStreamReader` uses a separate incremental tokenizer (`StreamTokenizer`) that operates on a single-character lookahead from a `string_view` over the entire file content (which is read in full on `Open`). Rather than building a token vector upfront, it produces tokens on demand during `NextObject` and `ReadRow` calls.

`NextObject` advances past any remaining rows of the previous object, then calls `ParseObjectHeader` to read the name, row count, and column schema. `ReadRow` calls `ReadValue` for each column in turn. Object references remain as `OTNObjectRef` values — they are never resolved, so `OBJECT_REF` columns must be interpreted by the caller using their integer indices.
