#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <cassert>
#include <cstdlib>

#include "OTNFile.h"

namespace OTN {

	static inline void HashCombine(size_t& seed, size_t value) {
		seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
	}

	// returns OTNBasType or UNKNOWN
	OTNBaseType StringToOTNBaseType(const std::string& type) {
		namespace SynTypes = Syntax::Types;

		static const std::unordered_map<std::string_view, OTNBaseType> table = {
			{ SynTypes::INT, OTNBaseType::INT },
			{ SynTypes::INT64, OTNBaseType::INT64 },
			{ SynTypes::UINT64, OTNBaseType::UINT64 },
			{ SynTypes::FLOAT, OTNBaseType::FLOAT },
			{ SynTypes::DOUBLE, OTNBaseType::DOUBLE },
			{ SynTypes::BOOL, OTNBaseType::BOOL },
			{ SynTypes::STRING, OTNBaseType::STRING },
			{ SynTypes::OBJECT, OTNBaseType::OBJECT },
			{ SynTypes::OBJECT_REF, OTNBaseType::OBJECT_REF },
			{ SynTypes::LIST, OTNBaseType::LIST },
			{ SynTypes::ANY, OTNBaseType::ANY }
		};

		auto it = table.find(type);
		return it != table.end() ? it->second : OTNBaseType::UNKNOWN;
	}

	constexpr std::string_view OTNValueTypeToString(OTNBaseType type) noexcept {
		namespace SynTypes = Syntax::Types;

		switch (type) {
		case OTN::OTNBaseType::INT: 		return SynTypes::INT;
		case OTN::OTNBaseType::INT64:   	return SynTypes::INT64;
		case OTN::OTNBaseType::UINT64:     	return SynTypes::UINT64;
		case OTN::OTNBaseType::FLOAT:   	return SynTypes::FLOAT;
		case OTN::OTNBaseType::DOUBLE:  	return SynTypes::DOUBLE;
		case OTN::OTNBaseType::BOOL:    	return SynTypes::BOOL;
		case OTN::OTNBaseType::STRING:  	return SynTypes::STRING;
		case OTN::OTNBaseType::OBJECT:  	return SynTypes::OBJECT;
		case OTN::OTNBaseType::OBJECT_REF: 	return SynTypes::OBJECT_REF;
		case OTN::OTNBaseType::LIST:    	return SynTypes::LIST;
		case OTN::OTNBaseType::ANY:    		return SynTypes::ANY;
		case OTN::OTNBaseType::UNKNOWN:
		default:                         	return "UNKNOWN";
		}
	}

	constexpr uint32_t OTNValueTypeCharLength(OTNBaseType type) noexcept {
		return static_cast<uint32_t>(OTNValueTypeToString(type).size());
	}

	std::string TypeDescToString(const OTNTypeDesc& type) {
		std::string result;

		// Base type
		switch (type.baseType) {
		case OTNBaseType::INT:
		case OTNBaseType::INT64:
		case OTNBaseType::UINT64:
		case OTNBaseType::FLOAT:
		case OTNBaseType::DOUBLE:
		case OTNBaseType::BOOL:
		case OTNBaseType::STRING:
			result = OTNValueTypeToString(type.baseType);
			break;

		case OTNBaseType::OBJECT:
			// Use referenced object name if available
			if (!type.refObjectName.empty())
				result = type.refObjectName;
			else
				result = "object";
			break;
		case OTNBaseType::ANY:
			result = "any";
			break;
		case OTNBaseType::OBJECT_REF:
		case OTNBaseType::UNKNOWN:
		default:
			result = "unknown";
			break;
		}

		// Append list depth
		for (uint32_t i = 0; i < type.listDepth; ++i) {
			result += "[]";
		}

		return result;
	}

	// parses a number of an 'any' value (sign included in text)
	// integers become int, int64 or uint64 (first that fits), all other numbers become double
	static bool ParseAnyNumber(const std::string& text, OTNValue& out) {
		const char* begin = text.data();
		const char* end = begin + text.size();

		if (text.find_first_of(".eE") == std::string::npos) {
			int i = 0;
			auto r = std::from_chars(begin, end, i);
			if (r.ec == std::errc() && r.ptr == end) {
				out = OTNValue(i);
				return true;
			}

			int64_t i64 = 0;
			r = std::from_chars(begin, end, i64);
			if (r.ec == std::errc() && r.ptr == end) {
				out = OTNValue(i64);
				return true;
			}

			uint64_t u64 = 0;
			r = std::from_chars(begin, end, u64);
			if (r.ec == std::errc() && r.ptr == end) {
				out = OTNValue(u64);
				return true;
			}
			return false;
		}

		double d = 0.0;
		auto r = std::from_chars(begin, end, d);
		if (r.ec != std::errc() || r.ptr != end)
			return false;

		out = OTNValue(d);
		return true;
	}

	// parses an unsigned id/count token without throwing
	static bool ParseUInt32(const std::string& text, uint32_t& out) {
		const char* end = text.data() + text.size();
		auto r = std::from_chars(text.data(), end, out);
		return r.ec == std::errc() && r.ptr == end;
	}

	// parses an unsigned row index token without throwing
	static bool ParseIndex(const std::string& text, size_t& out) {
		const char* end = text.data() + text.size();
		auto r = std::from_chars(text.data(), end, out);
		return r.ec == std::errc() && r.ptr == end;
	}

	constexpr bool CREATE_MISSING_DIR = true;/* < Helper to for ValidateFilePath, should be used instead of true or false */
	static bool ValidateFilePath(const OTNFilePath& path, bool createMissingDir, OTNFilePath& out, std::string& errorOut) {
		namespace fs = std::filesystem;

		fs::path finalPath = path;

		// Check file name
		if (finalPath.filename().empty()) {
			errorOut = "File path has no file name!";
			return false;
		}

		// Ensure parent directory exists (create if missing)
		fs::path parentDir = finalPath.parent_path();
		if (!parentDir.empty() && !fs::exists(parentDir)) {
			if (!createMissingDir) {
				errorOut = "File path does not exist!";
				return false;
			}

			std::error_code ec;
			if (!fs::create_directories(parentDir, ec)) {
				errorOut = "Failed to create directory '" + parentDir.string() +
					"': " + ec.message();
				return false;
			}
		}

		// File extension check
		if (finalPath.has_extension()) {
			std::string ext = finalPath.extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

			if (ext != FILE_EXTENSION) {
				std::string validExt{ FILE_EXTENSION };
				std::string validExtUpper = validExt;
				std::transform(validExtUpper.begin(), validExtUpper.end(),
					validExtUpper.begin(), ::toupper);

				errorOut = "File extension '" + ext +
					"' is invalid, valid extensions are " +
					validExtUpper + ", " + validExt + "!";
				return false;
			}
		}
		else {
			// Append default extension
			finalPath += OTN::FILE_EXTENSION;
		}

		out = finalPath;
		return true;
	}

	static OTNTypeDesc DeduceTypeFromValue(const OTNValue& val) {
		OTNTypeDesc desc;
		desc.baseType = val.type;

		const OTNValue* current = &val;
		while (current->type == OTNBaseType::LIST) {
			desc.listDepth++;
			const auto& arr = std::get<Detail::OTNArrayPtr>(current->value);
			if (!arr || arr->values.empty()) {
				desc.baseType = OTNBaseType::UNKNOWN;
				break;
			}
			current = &arr->values.front();
			desc.baseType = current->type;
		}

		if (desc.baseType == OTNBaseType::OBJECT) {
			const auto& obj = std::get<Detail::OTNObjectPtr>(current->value);
			if (obj)
				desc.refObjectName = obj->GetObjectName();
		}

		if (desc.baseType == OTNBaseType::OBJECT_REF) {
			const auto& obj = std::get<Detail::OTNObjectRef>(current->value);
			desc.baseType = OTNBaseType::OBJECT;
			desc.refObjectName = obj.refObjectName;
		}

		return desc;
	}

#pragma region Syntax

	namespace Detail {

		void DefTable::Add(uint32_t id, const std::string& name) {
			m_names[id] = name;
			m_ids[name] = id;
		}

		void DefTable::Clear() {
			m_names.clear();
			m_ids.clear();
		}

		bool DefTable::Empty() const {
			return m_names.empty();
		}

		const std::string* DefTable::FindName(uint32_t id) const {
			auto it = m_names.find(id);
			return it != m_names.end() ? &it->second : nullptr;
		}

		std::optional<uint32_t> DefTable::FindId(const std::string& name) const {
			auto it = m_ids.find(name);
			if (it == m_ids.end())
				return std::nullopt;
			return it->second;
		}

		std::vector<std::pair<uint32_t, std::string>> DefTable::GetEntries() const {
			std::vector<std::pair<uint32_t, std::string>> entries(m_names.begin(), m_names.end());
			std::sort(entries.begin(), entries.end(),
				[](const auto& a, const auto& b) { return a.first < b.first; });
			return entries;
		}

		// Parses a type string like "int", "float[]", "MyObject" or "MyObject[][]".
		// Unknown base types are references to the object with this name.
		static std::optional<OTNTypeDesc> ParseTypeString(const std::string& typeStr) {
			uint32_t listDepth = 0;
			size_t pos = typeStr.size();

			// Count trailing [] pairs
			while (pos >= 2 &&
				typeStr[pos - 2] == Syntax::LIST_BEGIN_CHAR &&
				typeStr[pos - 1] == Syntax::LIST_END_CHAR)
			{
				listDepth++;
				pos -= 2;
			}

			if (pos == 0)
				return std::nullopt;

			std::string baseTypeStr = typeStr.substr(0, pos);
			OTNTypeDesc desc(StringToOTNBaseType(baseTypeStr), listDepth);
			if (desc.baseType == OTNBaseType::UNKNOWN) {
				desc.baseType = OTNBaseType::OBJECT;
				desc.refObjectName = std::move(baseTypeStr);
			}
			return desc;
		}

		// Character input from memory or buffered from a file, supports seeking.
		class CharSource {
		public:
			void OpenMemory(std::string_view data) {
				Close();
				m_data = data.data();
				m_size = data.size();
			}

			bool OpenFile(const OTNFilePath& path) {
				Close();
				m_file.open(path, std::ios::binary);
				if (!m_file.is_open())
					return false;

				m_isFile = true;
				m_buffer.resize(BUFFER_SIZE);
				return true;
			}

			void Close() {
				if (m_file.is_open())
					m_file.close();
				m_file.clear();
				m_isFile = false;
				m_data = nullptr;
				m_pos = 0;
				m_size = 0;
				m_bufferOffset = 0;
			}

			bool Get(char& c) {
				if (m_pos >= m_size && !Fill())
					return false;

				c = m_data[m_pos++];
				return true;
			}

			int Peek() {
				if (m_pos >= m_size && !Fill())
					return -1;

				return static_cast<unsigned char>(m_data[m_pos]);
			}

			// offset of the next char
			uint64_t Tell() const {
				return m_bufferOffset + m_pos;
			}

			bool Seek(uint64_t offset) {
				// inside the current data, no file access needed
				if (offset >= m_bufferOffset && offset <= m_bufferOffset + m_size) {
					m_pos = static_cast<size_t>(offset - m_bufferOffset);
					return true;
				}

				if (!m_isFile)
					return false;

				m_file.clear();
				m_file.seekg(static_cast<std::streamoff>(offset));
				if (!m_file)
					return false;

				m_bufferOffset = offset;
				m_pos = 0;
				m_size = 0;
				return true;
			}

		private:
			static constexpr size_t BUFFER_SIZE = 64 * 1024; // 64 KB

			std::ifstream m_file;
			bool m_isFile = false;
			std::vector<char> m_buffer;
			const char* m_data = nullptr;  // memory data or m_buffer
			size_t m_pos = 0;
			size_t m_size = 0;
			uint64_t m_bufferOffset = 0;   // file offset of m_data[0]

			bool Fill() {
				if (!m_isFile)
					return false;

				m_bufferOffset += m_size;
				m_pos = 0;
				m_file.read(m_buffer.data(), static_cast<std::streamsize>(m_buffer.size()));
				m_size = static_cast<size_t>(m_file.gcount());
				m_data = m_buffer.data();
				return m_size > 0;
			}
		};

		// ---- Tokenizer ----

		enum class TokenType {
			UNKNOWN = 0,
			KEYWORD_PREFIX, // @
			IDENTIFIER,     // object, version, Weapon
			NUMBER,         // 123
			STRING,         // "Metal"
			MINUS,          // -
			COLON,          // :
			EQUALS,         // =
			COMMA,          // ,
			SLASH,          // /
			SEMICOLON,      // ;
			BLOCK_BEGIN,    // {
			BLOCK_END,      // }
			LIST_BEGIN,     // [
			LIST_END,       // ]
			REF_BEGIN,      // <
			REF_END,        // >
			END_OF_FILE
		};

		static std::string TokenTypeToString(TokenType type) {
			switch (type) {
			case TokenType::KEYWORD_PREFIX: return std::string("'") + Syntax::KEYWORD_PREFIX_CHAR + "'";
			case TokenType::IDENTIFIER:     return "IDENTIFIER";
			case TokenType::NUMBER:         return "NUMBER";
			case TokenType::STRING:         return "STRING";
			case TokenType::MINUS:          return "'-'";
			case TokenType::COLON:          return std::string("'") + Syntax::KEYWORD_ASSIGN_CHAR + "'";
			case TokenType::EQUALS:         return std::string("'") + Syntax::ASSIGNMENT_CHAR + "'";
			case TokenType::COMMA:          return std::string("'") + Syntax::SEPARATOR_CHAR + "'";
			case TokenType::SLASH:          return std::string("'") + Syntax::TYPE_SEPARATOR_CHAR + "'";
			case TokenType::SEMICOLON:      return std::string("'") + Syntax::STATEMENT_TERMINATOR + "'";
			case TokenType::BLOCK_BEGIN:    return std::string("'") + Syntax::BLOCK_BEGIN_CHAR + "'";
			case TokenType::BLOCK_END:      return std::string("'") + Syntax::BLOCK_END_CHAR + "'";
			case TokenType::LIST_BEGIN:     return std::string("'") + Syntax::LIST_BEGIN_CHAR + "'";
			case TokenType::LIST_END:       return std::string("'") + Syntax::LIST_END_CHAR + "'";
			case TokenType::REF_BEGIN:      return std::string("'") + Syntax::REF_BEGIN_CHAR + "'";
			case TokenType::REF_END:        return std::string("'") + Syntax::REF_END_CHAR + "'";
			case TokenType::END_OF_FILE:    return "END_OF_FILE";
			case TokenType::UNKNOWN:
			default:                        return "UNKNOWN";
			}
		}

		struct Token {
			TokenType type = TokenType::UNKNOWN;
			std::string text;
			FilePosition pos;
		};

		// Splits the chars of a CharSource into tokens, one token at a time.
		class Tokenizer {
		public:
			explicit Tokenizer(CharSource& src)
				: m_src(src) {
			}

			Token Next() {
				if (m_peeked.has_value()) {
					Token t = std::move(*m_peeked);
					m_peeked.reset();
					return t;
				}
				return ReadToken();
			}

			const Token& Peek() {
				if (!m_peeked.has_value())
					m_peeked = ReadToken();
				return *m_peeked;
			}

			bool NextIf(TokenType type) {
				if (Peek().type != type)
					return false;

				Next();
				return true;
			}

			// consumes the next token, adds an error if it has another type
			bool Expect(TokenType type, Token* outToken = nullptr) {
				Token tok = Next();
				if (tok.type != type) {
					AddError(tok, "unexpected token " + Describe(tok) + ", expected " + TokenTypeToString(type));
					return false;
				}

				if (outToken)
					*outToken = std::move(tok);
				return true;
			}

			// position of the next token
			FilePosition Tell() const {
				if (m_peeked.has_value())
					return m_peeked->pos;
				return CurrentPosition();
			}

			void Seek(const FilePosition& pos) {
				m_peeked.reset();
				if (!m_src.Seek(pos.offset)) {
					AddError("failed to seek to offset " + std::to_string(pos.offset));
					return;
				}
				m_line = pos.line;
				m_column = pos.column;
			}

			bool IsValid() const {
				return m_valid;
			}

			const std::string& GetError() const {
				return m_error;
			}

			void AddError(const std::string& msg) {
				m_valid = false;
				if (!m_error.empty())
					m_error.push_back('\n');
				m_error += msg;
			}

			void AddError(const Token& token, const std::string& msg) {
				AddError("Error at line " + std::to_string(token.pos.line) +
					", column " + std::to_string(token.pos.column) + ": " + msg);
			}

			static std::string Describe(const Token& tok) {
				if (tok.type == TokenType::IDENTIFIER || tok.type == TokenType::NUMBER || tok.type == TokenType::STRING)
					return TokenTypeToString(tok.type) + "(\"" + tok.text + "\")";
				return TokenTypeToString(tok.type);
			}

		private:
			CharSource& m_src;
			std::optional<Token> m_peeked;
			bool m_valid = true;
			std::string m_error;
			uint32_t m_line = 1;
			uint32_t m_column = 1;

			FilePosition CurrentPosition() const {
				return { m_src.Tell(), m_line, m_column };
			}

			void Advance(char c) {
				if (c == '\n') {
					m_line++;
					m_column = 1;
				}
				else {
					m_column++;
				}
			}

			bool GetChar(char& c) {
				if (!m_src.Get(c))
					return false;
				Advance(c);
				return true;
			}

			void SkipWhitespace() {
				int c;
				char ch;
				while ((c = m_src.Peek()) != -1 && std::isspace(c))
					GetChar(ch);
			}

			Token MakeToken(TokenType type, const FilePosition& pos, std::string text = {}) const {
				Token t;
				t.type = type;
				t.text = std::move(text);
				t.pos = pos;
				return t;
			}

			Token ReadToken() {
				SkipWhitespace();

				const FilePosition start = CurrentPosition();
				int next = m_src.Peek();
				if (next == -1)
					return MakeToken(TokenType::END_OF_FILE, start);

				if (std::isdigit(next))
					return ReadNumber(start);

				if (std::isalpha(next) || next == '_')
					return ReadIdentifier(start);

				char c;
				GetChar(c);

				switch (c) {
				case Syntax::STATEMENT_TERMINATOR: return MakeToken(TokenType::SEMICOLON, start);
				case Syntax::KEYWORD_PREFIX_CHAR:  return MakeToken(TokenType::KEYWORD_PREFIX, start);
				case Syntax::KEYWORD_ASSIGN_CHAR:  return MakeToken(TokenType::COLON, start);
				case Syntax::ASSIGNMENT_CHAR:      return MakeToken(TokenType::EQUALS, start);
				case Syntax::SEPARATOR_CHAR:       return MakeToken(TokenType::COMMA, start);
				case Syntax::TYPE_SEPARATOR_CHAR:  return MakeToken(TokenType::SLASH, start);
				case Syntax::BLOCK_BEGIN_CHAR:     return MakeToken(TokenType::BLOCK_BEGIN, start);
				case Syntax::BLOCK_END_CHAR:       return MakeToken(TokenType::BLOCK_END, start);
				case Syntax::LIST_BEGIN_CHAR:      return MakeToken(TokenType::LIST_BEGIN, start);
				case Syntax::LIST_END_CHAR:        return MakeToken(TokenType::LIST_END, start);
				case Syntax::REF_BEGIN_CHAR:       return MakeToken(TokenType::REF_BEGIN, start);
				case Syntax::REF_END_CHAR:         return MakeToken(TokenType::REF_END, start);
				case '-':                          return MakeToken(TokenType::MINUS, start);
				case '"':                          return ReadString(start);
				default: {
					Token t = MakeToken(TokenType::UNKNOWN, start, std::string(1, c));
					AddError(t, std::string("unexpected character '") + c + "'");
					return t;
				}
				}
			}

			Token ReadString(const FilePosition& start) {
				std::string text;
				text.reserve(32);

				char c;
				while (GetChar(c)) {
					if (c == '"')
						return MakeToken(TokenType::STRING, start, std::move(text));

					if (c != '\\') {
						text.push_back(c);
						continue;
					}

					if (!GetChar(c))
						break;

					switch (c) {
					case 'n':  text.push_back('\n'); break;
					case 't':  text.push_back('\t'); break;
					case '"':  text.push_back('"');  break;
					case '\\': text.push_back('\\'); break;
					default: {
						Token t = MakeToken(TokenType::UNKNOWN, start);
						AddError(t, std::string("invalid escape sequence '\\") + c + "'");
						return t;
					}
					}
				}

				Token t = MakeToken(TokenType::UNKNOWN, start);
				AddError(t, "unterminated string literal");
				return t;
			}

			Token ReadNumber(const FilePosition& start) {
				std::string text;
				bool hasDot = false;
				bool hasExp = false;

				char c;
				int next;
				while ((next = m_src.Peek()) != -1) {
					if (next == '.') {
						if (hasDot || hasExp)
							break;
						hasDot = true;
					}
					else if (next == 'e' || next == 'E') {
						if (hasExp)
							break;
						hasExp = true;

						GetChar(c);
						text.push_back(c);

						// exponent can have a sign
						int sign = m_src.Peek();
						if (sign == '+' || sign == '-') {
							GetChar(c);
							text.push_back(c);
						}
						continue;
					}
					else if (!std::isdigit(next)) {
						break;
					}

					GetChar(c);
					text.push_back(c);
				}

				return MakeToken(TokenType::NUMBER, start, std::move(text));
			}

			Token ReadIdentifier(const FilePosition& start) {
				std::string text;
				char c;
				int next;
				while ((next = m_src.Peek()) != -1 && (std::isalnum(next) || next == '_')) {
					GetChar(c);
					text.push_back(c);
				}

				return MakeToken(TokenType::IDENTIFIER, start, std::move(text));
			}
		};

		// Read functions of one file version.
		// A newer version can copy the table of an older one and replace single entries.
		struct ReadSyntax {
			// everything after @version up to and including the start of the @object block
			std::optional<FileHeaderInfo>(*readFileHeader)(Tokenizer& tok, uint8_t version);
			// true if another object follows inside the @object block
			bool (*hasMoreObjects)(Tokenizer& tok);
			std::optional<ObjectHeaderInfo>(*readObjectHeader)(Tokenizer& tok, const FileHeaderInfo& file);
			std::optional<OTNRow>(*readRow)(Tokenizer& tok, const std::vector<OTNTypeDesc>& types);
			bool (*skipRow)(Tokenizer& tok);
			// end of the @object block, nothing may follow
			bool (*readObjectBlockEnd)(Tokenizer& tok);
		};

		// Reads "@version: N;". Every OTN file starts with it, independent of its version.
		static std::optional<uint8_t> ReadVersion(Tokenizer& tok) {
			Token keyword;
			Token number;
			if (!tok.Expect(TokenType::KEYWORD_PREFIX) || !tok.Expect(TokenType::IDENTIFIER, &keyword))
				return std::nullopt;

			if (keyword.text != Keyword::VERSION_KW) {
				tok.AddError(keyword, "file has to start with '" +
					std::string(1, Syntax::KEYWORD_PREFIX_CHAR) + std::string(Keyword::VERSION_KW) + "'");
				return std::nullopt;
			}

			if (!tok.Expect(TokenType::COLON) ||
				!tok.Expect(TokenType::NUMBER, &number) ||
				!tok.Expect(TokenType::SEMICOLON))
			{
				return std::nullopt;
			}

			uint32_t version = 0;
			if (!ParseUInt32(number.text, version) || version == 0 || version > std::numeric_limits<uint8_t>::max()) {
				tok.AddError(number, "invalid version number '" + number.text + "'");
				return std::nullopt;
			}
			return static_cast<uint8_t>(version);
		}

		// ======== Version 1 ========
		namespace V1 {

			// ---------------- reading ----------------

			template<typename T>
			static bool ParseNumber(const std::string& text, T& out) {
				const char* end = text.data() + text.size();
				auto r = std::from_chars(text.data(), end, out);
				return r.ec == std::errc() && r.ptr == end;
			}

			// [-]NUMBER, returns the number text including the sign
			static bool ReadNumberText(Tokenizer& tok, std::string& outText, Token& outToken) {
				bool negative = tok.NextIf(TokenType::MINUS);
				if (!tok.Expect(TokenType::NUMBER, &outToken))
					return false;

				outText = negative ? "-" + outToken.text : outToken.text;
				return true;
			}

			template<typename T>
			static std::optional<OTNValue> ReadNumberValue(Tokenizer& tok, OTNBaseType type) {
				std::string text;
				Token numberToken;
				if (!ReadNumberText(tok, text, numberToken))
					return std::nullopt;

				T value{};
				if (!ParseNumber(text, value)) {
					tok.AddError(numberToken, "invalid " + std::string(OTNValueTypeToString(type)) + " literal '" + text + "'");
					return std::nullopt;
				}
				return OTNValue(value);
			}

			static std::optional<OTNValue> ReadValue(Tokenizer& tok, const OTNTypeDesc& type);

			static std::optional<OTNValue> ReadPrimitive(Tokenizer& tok, OTNBaseType type) {
				switch (type) {
				case OTNBaseType::INT:    return ReadNumberValue<int>(tok, type);
				case OTNBaseType::INT64:  return ReadNumberValue<int64_t>(tok, type);
				case OTNBaseType::UINT64: return ReadNumberValue<uint64_t>(tok, type);
				case OTNBaseType::FLOAT:  return ReadNumberValue<float>(tok, type);
				case OTNBaseType::DOUBLE: return ReadNumberValue<double>(tok, type);
				case OTNBaseType::BOOL: {
					Token t = tok.Next();
					if (t.type == TokenType::IDENTIFIER) {
						if (t.text == Keyword::TRUE_KW)
							return OTNValue(true);
						if (t.text == Keyword::FALSE_KW)
							return OTNValue(false);
					}
					else if (t.type == TokenType::NUMBER) {
						int64_t number = 0;
						if (ParseNumber(t.text, number))
							return OTNValue(number != 0);
					}

					tok.AddError(t, "invalid boolean literal " + Tokenizer::Describe(t));
					return std::nullopt;
				}
				case OTNBaseType::STRING: {
					Token t;
					if (!tok.Expect(TokenType::STRING, &t))
						return std::nullopt;
					return OTNValue(t.text);
				}
				default:
					tok.AddError(tok.Peek(), "unsupported column type '" + std::string(OTNValueTypeToString(type)) + "'");
					return std::nullopt;
				}
			}

			// object reference: row index of the referenced object
			static std::optional<OTNValue> ReadObjectRef(Tokenizer& tok, const std::string& objectName) {
				Token t;
				if (!tok.Expect(TokenType::NUMBER, &t))
					return std::nullopt;

				size_t index = 0;
				if (!ParseIndex(t.text, index)) {
					tok.AddError(t, "invalid object reference index '" + t.text + "'");
					return std::nullopt;
				}
				return OTNValue(OTNObjectRef(objectName, index));
			}

			static std::optional<OTNValue> ReadList(Tokenizer& tok, const OTNTypeDesc& elemType) {
				if (!tok.Expect(TokenType::LIST_BEGIN))
					return std::nullopt;

				auto list = std::make_shared<OTNArray>();
				while (tok.Peek().type != TokenType::LIST_END) {
					if (tok.Peek().type == TokenType::END_OF_FILE) {
						tok.AddError(tok.Peek(), "unexpected end of file in list");
						return std::nullopt;
					}

					std::optional<OTNValue> value = ReadValue(tok, elemType);
					if (!value)
						return std::nullopt;
					list->values.push_back(std::move(*value));

					if (!tok.NextIf(TokenType::COMMA))
						break;
				}

				if (!tok.Expect(TokenType::LIST_END))
					return std::nullopt;
				return OTNValue(list);
			}

			// value of an 'any' column, the type is taken from the value itself
			static std::optional<OTNValue> ReadAnyValue(Tokenizer& tok) {
				const TokenType type = tok.Peek().type;
				switch (type) {
				case TokenType::STRING:
					return OTNValue(tok.Next().text);

				case TokenType::IDENTIFIER: {
					Token t = tok.Next();
					if (t.text == Keyword::TRUE_KW)
						return OTNValue(true);
					if (t.text == Keyword::FALSE_KW)
						return OTNValue(false);

					tok.AddError(t, "unexpected identifier '" + t.text + "' in 'any' value");
					return std::nullopt;
				}

				case TokenType::NUMBER:
				case TokenType::MINUS: {
					std::string text;
					Token numberToken;
					if (!ReadNumberText(tok, text, numberToken))
						return std::nullopt;

					OTNValue value;
					if (!ParseAnyNumber(text, value)) {
						tok.AddError(numberToken, "invalid numeric literal '" + text + "' in 'any' value");
						return std::nullopt;
					}
					return value;
				}

				case TokenType::LIST_BEGIN:
					return ReadList(tok, OTNTypeDesc(OTNBaseType::ANY, 0));

				default: {
					Token t = tok.Next();
					tok.AddError(t, "unexpected token " + Tokenizer::Describe(t) + " in 'any' value");
					return std::nullopt;
				}
				}
			}

			static std::optional<OTNValue> ReadValue(Tokenizer& tok, const OTNTypeDesc& type) {
				if (type.listDepth > 0) {
					OTNTypeDesc elemType = type;
					elemType.listDepth--;
					return ReadList(tok, elemType);
				}

				switch (type.baseType) {
				case OTNBaseType::ANY:    return ReadAnyValue(tok);
				case OTNBaseType::OBJECT: return ReadObjectRef(tok, type.refObjectName);
				default:                  return ReadPrimitive(tok, type.baseType);
				}
			}

			// ": name = id, name = id;" of @defType / @defName
			static bool ReadDefinitions(Tokenizer& tok, DefTable& outTable, bool isTypeTable) {
				if (!tok.Expect(TokenType::COLON))
					return false;

				do {
					Token name;
					Token number;
					if (!tok.Expect(TokenType::IDENTIFIER, &name) ||
						!tok.Expect(TokenType::EQUALS) ||
						!tok.Expect(TokenType::NUMBER, &number))
					{
						return false;
					}

					if (isTypeTable && StringToOTNBaseType(name.text) == OTNBaseType::UNKNOWN) {
						tok.AddError(name, "unknown type '" + name.text + "'");
						return false;
					}

					uint32_t id = 0;
					if (!ParseUInt32(number.text, id)) {
						tok.AddError(number, "invalid id '" + number.text + "'");
						return false;
					}
					outTable.Add(id, name.text);
				} while (tok.NextIf(TokenType::COMMA));

				return tok.Expect(TokenType::SEMICOLON);
			}

			static std::optional<FileHeaderInfo> ReadFileHeader(Tokenizer& tok, uint8_t version) {
				FileHeaderInfo info;
				info.version = version;

				while (true) {
					Token keyword;
					if (!tok.Expect(TokenType::KEYWORD_PREFIX) || !tok.Expect(TokenType::IDENTIFIER, &keyword))
						return std::nullopt;

					if (keyword.text == Keyword::DEF_TYPE_KW) {
						if (!ReadDefinitions(tok, info.defType, true))
							return std::nullopt;
					}
					else if (keyword.text == Keyword::DEF_NAME_KW) {
						if (!ReadDefinitions(tok, info.defName, false))
							return std::nullopt;
					}
					else if (keyword.text == Keyword::OBJECT_KW) {
						if (!tok.Expect(TokenType::COLON) || !tok.Expect(TokenType::BLOCK_BEGIN))
							return std::nullopt;
						return info;
					}
					else {
						tok.AddError(keyword, "unknown directive '" + std::string(1, Syntax::KEYWORD_PREFIX_CHAR) + keyword.text + "'");
						return std::nullopt;
					}
				}
			}

			static bool HasMoreObjects(Tokenizer& tok) {
				if (!tok.IsValid())
					return false;

				const TokenType type = tok.Peek().type;
				return type != TokenType::BLOCK_END && type != TokenType::END_OF_FILE;
			}

			static bool ReadObjectBlockEnd(Tokenizer& tok) {
				if (!tok.Expect(TokenType::BLOCK_END) || !tok.Expect(TokenType::SEMICOLON))
					return false;

				if (tok.Peek().type != TokenType::END_OF_FILE) {
					tok.AddError(tok.Peek(), "unexpected content after the @object block");
					return false;
				}
				return true;
			}

			// Ref<Name>, type name or @defType id, followed by [] per list depth
			static std::optional<OTNTypeDesc> ReadColumnType(Tokenizer& tok, const FileHeaderInfo& file) {
				Token t = tok.Next();
				std::optional<OTNTypeDesc> type;

				if (t.type == TokenType::IDENTIFIER && t.text == Keyword::REF_KW) {
					Token name;
					if (!tok.Expect(TokenType::REF_BEGIN) ||
						!tok.Expect(TokenType::IDENTIFIER, &name) ||
						!tok.Expect(TokenType::REF_END))
					{
						return std::nullopt;
					}
					type = OTNTypeDesc(name.text);
				}
				else if (t.type == TokenType::IDENTIFIER) {
					type = ParseTypeString(t.text);
				}
				else if (t.type == TokenType::NUMBER) {
					uint32_t id = 0;
					const std::string* name = ParseUInt32(t.text, id) ? file.defType.FindName(id) : nullptr;
					if (!name) {
						tok.AddError(t, "unknown @defType id '" + t.text + "'");
						return std::nullopt;
					}
					type = ParseTypeString(*name);
				}

				if (!type) {
					tok.AddError(t, "expected column type, got " + Tokenizer::Describe(t));
					return std::nullopt;
				}

				while (tok.NextIf(TokenType::LIST_BEGIN)) {
					if (!tok.Expect(TokenType::LIST_END))
						return std::nullopt;
					type->listDepth++;
				}
				return type;
			}

			// column name or @defName id
			static std::optional<std::string> ReadColumnName(Tokenizer& tok, const FileHeaderInfo& file) {
				Token t = tok.Next();
				if (t.type == TokenType::IDENTIFIER)
					return t.text;

				if (t.type == TokenType::NUMBER) {
					uint32_t id = 0;
					const std::string* name = ParseUInt32(t.text, id) ? file.defName.FindName(id) : nullptr;
					if (name)
						return *name;

					tok.AddError(t, "unknown @defName id '" + t.text + "'");
					return std::nullopt;
				}

				tok.AddError(t, "expected column name, got " + Tokenizer::Describe(t));
				return std::nullopt;
			}

			// Name[rowCount] { type/name, ... };
			static std::optional<ObjectHeaderInfo> ReadObjectHeader(Tokenizer& tok, const FileHeaderInfo& file) {
				Token name;
				Token count;
				if (!tok.Expect(TokenType::IDENTIFIER, &name) ||
					!tok.Expect(TokenType::LIST_BEGIN) ||
					!tok.Expect(TokenType::NUMBER, &count) ||
					!tok.Expect(TokenType::LIST_END) ||
					!tok.Expect(TokenType::BLOCK_BEGIN))
				{
					return std::nullopt;
				}

				ObjectHeaderInfo info;
				info.name = name.text;
				if (!ParseIndex(count.text, info.rowCount)) {
					tok.AddError(count, "invalid row count '" + count.text + "'");
					return std::nullopt;
				}

				while (tok.Peek().type != TokenType::BLOCK_END) {
					std::optional<OTNTypeDesc> type = ReadColumnType(tok, file);
					if (!type || !tok.Expect(TokenType::SLASH))
						return std::nullopt;

					std::optional<std::string> columnName = ReadColumnName(tok, file);
					if (!columnName)
						return std::nullopt;

					info.columnTypes.push_back(std::move(*type));
					info.columnNames.push_back(std::move(*columnName));

					if (!tok.NextIf(TokenType::COMMA))
						break;
				}

				if (!tok.Expect(TokenType::BLOCK_END) || !tok.Expect(TokenType::SEMICOLON))
					return std::nullopt;
				return info;
			}

			// value, value, ...;
			static std::optional<OTNRow> ReadRow(Tokenizer& tok, const std::vector<OTNTypeDesc>& types) {
				OTNRow row;
				row.reserve(types.size());

				for (size_t col = 0; col < types.size(); ++col) {
					if (col > 0 && !tok.Expect(TokenType::COMMA))
						return std::nullopt;

					std::optional<OTNValue> value = ReadValue(tok, types[col]);
					if (!value)
						return std::nullopt;
					row.push_back(std::move(*value));
				}

				if (!tok.Expect(TokenType::SEMICOLON))
					return std::nullopt;
				return row;
			}

			static bool SkipRow(Tokenizer& tok) {
				size_t depth = 0;
				while (tok.IsValid()) {
					Token t = tok.Next();
					switch (t.type) {
					case TokenType::END_OF_FILE:
						tok.AddError(t, "unexpected end of file in row");
						return false;
					case TokenType::LIST_BEGIN:
					case TokenType::BLOCK_BEGIN:
						depth++;
						break;
					case TokenType::LIST_END:
					case TokenType::BLOCK_END:
						if (depth > 0)
							depth--;
						break;
					case TokenType::SEMICOLON:
						if (depth == 0)
							return tok.IsValid();
						break;
					default:
						break;
					}
				}
				return false;
			}

			static const ReadSyntax READ_SYNTAX = {
				&ReadFileHeader,
				&HasMoreObjects,
				&ReadObjectHeader,
				&ReadRow,
				&SkipRow,
				&ReadObjectBlockEnd
			};

			// ---------------- writing ----------------

			static void AddSpace(std::string& out, const WriteOptions& options) {
				if (!options.compact)
					out.push_back(' ');
			}

			static void AddLineBreak(std::string& out, const WriteOptions& options) {
				if (!options.compact)
					out.push_back('\n');
			}

			static void AddIndent(std::string& out, const WriteOptions& options, uint32_t level) {
				if (!options.compact)
					out.append(level, '\t');
			}

			template<typename T>
			static void AppendNumber(std::string& out, T value) {
				char buf[64];
				std::to_chars_result r;
				if constexpr (std::is_floating_point_v<T>)
					r = std::to_chars(buf, buf + sizeof(buf), value,
						std::chars_format::general, std::numeric_limits<T>::max_digits10);
				else
					r = std::to_chars(buf, buf + sizeof(buf), value);

				if (r.ec == std::errc())
					out.append(buf, r.ptr);
			}

			// string literal including quotes, escapes chars the reader treats as escape sequences
			static void AppendEscapedString(std::string& out, const std::string& value) {
				out.push_back('"');
				for (char c : value) {
					switch (c) {
					case '"':  out += "\\\""; break;
					case '\\': out += "\\\\"; break;
					case '\n': out += "\\n";  break;
					case '\t': out += "\\t";  break;
					default:   out.push_back(c); break;
					}
				}
				out.push_back('"');
			}

			// "@keyword: "
			static void BeginDirective(std::string& out, const WriteOptions& options, std::string_view keyword) {
				out.push_back(Syntax::KEYWORD_PREFIX_CHAR);
				out.append(keyword);
				out.push_back(Syntax::KEYWORD_ASSIGN_CHAR);
				AddSpace(out, options);
			}

			static void EndStatement(std::string& out, const WriteOptions& options) {
				out.push_back(Syntax::STATEMENT_TERMINATOR);
				AddLineBreak(out, options);
			}

			// "@keyword: name = id, name = id;"
			static void WriteDefinitions(std::string& out, const WriteOptions& options, std::string_view keyword, const DefTable& table) {
				if (table.Empty())
					return;

				BeginDirective(out, options, keyword);
				bool first = true;
				for (const auto& [id, name] : table.GetEntries()) {
					if (!first) {
						out.push_back(Syntax::SEPARATOR_CHAR);
						AddSpace(out, options);
					}
					first = false;

					out += name;
					AddSpace(out, options);
					out.push_back(Syntax::ASSIGNMENT_CHAR);
					AddSpace(out, options);
					AppendNumber(out, id);
				}
				EndStatement(out, options);
			}

			static void WriteFileHeader(std::string& out, const WriteOptions& options, const FileHeaderInfo& info) {
				BeginDirective(out, options, Keyword::VERSION_KW);
				AppendNumber(out, static_cast<unsigned>(info.version));
				EndStatement(out, options);

				WriteDefinitions(out, options, Keyword::DEF_TYPE_KW, info.defType);
				WriteDefinitions(out, options, Keyword::DEF_NAME_KW, info.defName);
				AddLineBreak(out, options);
			}

			static void WriteObjectBlockBegin(std::string& out, const WriteOptions& options) {
				BeginDirective(out, options, Keyword::OBJECT_KW);
				out.push_back(Syntax::BLOCK_BEGIN_CHAR);
				AddLineBreak(out, options);
			}

			static void WriteObjectBlockEnd(std::string& out, const WriteOptions& options) {
				out.push_back(Syntax::BLOCK_END_CHAR);
				EndStatement(out, options);
			}

			// Ref<Name>, type name or @defType id, followed by [] per list depth
			static bool WriteColumnType(std::string& out, const OTNTypeDesc& type, const FileHeaderInfo& file, std::string& error) {
				if (!type.refObjectName.empty()) {
					out.append(Keyword::REF_KW);
					out.push_back(Syntax::REF_BEGIN_CHAR);
					out.append(type.refObjectName);
					out.push_back(Syntax::REF_END_CHAR);
				}
				else {
					switch (type.baseType) {
					case OTNBaseType::INT:
					case OTNBaseType::INT64:
					case OTNBaseType::UINT64:
					case OTNBaseType::FLOAT:
					case OTNBaseType::DOUBLE:
					case OTNBaseType::BOOL:
					case OTNBaseType::STRING:
					case OTNBaseType::ANY:
						break;
					default:
						error = "column type '" + TypeDescToString(type) + "' cannot be written";
						return false;
					}

					std::string name(OTNValueTypeToString(type.baseType));
					if (std::optional<uint32_t> id = file.defType.FindId(name))
						AppendNumber(out, *id);
					else
						out += name;
				}

				for (uint32_t i = 0; i < type.listDepth; ++i) {
					out.push_back(Syntax::LIST_BEGIN_CHAR);
					out.push_back(Syntax::LIST_END_CHAR);
				}
				return true;
			}

			// Name[rowCount] { type/name, ... };
			static bool WriteObjectHeader(std::string& out, const WriteOptions& options,
				const ObjectHeaderInfo& info, const FileHeaderInfo& file, std::string& error)
			{
				if (info.columnNames.size() != info.columnTypes.size()) {
					error = "size of names (" + std::to_string(info.columnNames.size()) +
						") and types (" + std::to_string(info.columnTypes.size()) +
						") does not match in object '" + info.name + "'";
					return false;
				}

				AddIndent(out, options, 1);
				out += info.name;
				out.push_back(Syntax::LIST_BEGIN_CHAR);
				AppendNumber(out, info.rowCount);
				out.push_back(Syntax::LIST_END_CHAR);
				AddSpace(out, options);
				out.push_back(Syntax::BLOCK_BEGIN_CHAR);
				AddLineBreak(out, options);

				AddIndent(out, options, 2);
				for (size_t i = 0; i < info.columnNames.size(); ++i) {
					if (i > 0) {
						out.push_back(Syntax::SEPARATOR_CHAR);
						AddSpace(out, options);
					}

					const std::string& columnName = info.columnNames[i];
					if (!WriteColumnType(out, info.columnTypes[i], file, error)) {
						error += " (column '" + columnName + "' of object '" + info.name + "')";
						return false;
					}

					out.push_back(Syntax::TYPE_SEPARATOR_CHAR);
					if (std::optional<uint32_t> id = file.defName.FindId(columnName))
						AppendNumber(out, *id);
					else
						out += columnName;
				}
				AddLineBreak(out, options);

				AddIndent(out, options, 1);
				out.push_back(Syntax::BLOCK_END_CHAR);
				EndStatement(out, options);
				return true;
			}

			static bool WriteValue(std::string& out, const WriteOptions& options, const OTNValue& value, std::string& error) {
				switch (value.type) {
				case OTNBaseType::INT:
					AppendNumber(out, std::get<int>(value.value));
					return true;
				case OTNBaseType::INT64:
					AppendNumber(out, std::get<int64_t>(value.value));
					return true;
				case OTNBaseType::UINT64:
					AppendNumber(out, std::get<uint64_t>(value.value));
					return true;
				case OTNBaseType::FLOAT:
					AppendNumber(out, std::get<float>(value.value));
					return true;
				case OTNBaseType::DOUBLE:
					AppendNumber(out, std::get<double>(value.value));
					return true;
				case OTNBaseType::BOOL:
					out.append(std::get<bool>(value.value) ? Keyword::TRUE_KW : Keyword::FALSE_KW);
					return true;
				case OTNBaseType::STRING:
					AppendEscapedString(out, std::get<std::string>(value.value));
					return true;
				case OTNBaseType::LIST: {
					const OTNArrayPtr& list = std::get<OTNArrayPtr>(value.value);
					out.push_back(Syntax::LIST_BEGIN_CHAR);
					if (list) {
						bool first = true;
						for (const OTNValue& elem : list->values) {
							if (!first) {
								out.push_back(Syntax::SEPARATOR_CHAR);
								AddSpace(out, options);
							}
							first = false;

							if (!WriteValue(out, options, elem, error))
								return false;
						}
					}
					out.push_back(Syntax::LIST_END_CHAR);
					return true;
				}
				default:
					error = "value of type '" + std::string(OTNValueTypeToString(value.type)) + "' cannot be written";
					return false;
				}
			}

			// value, value, ...;
			static bool WriteRow(std::string& out, const WriteOptions& options, const OTNRow& row, std::string& error) {
				AddIndent(out, options, 1);
				for (size_t i = 0; i < row.size(); ++i) {
					if (i > 0) {
						out.push_back(Syntax::SEPARATOR_CHAR);
						AddSpace(out, options);
					}

					if (!WriteValue(out, options, row[i], error))
						return false;
				}
				EndStatement(out, options);
				return true;
			}

		}

		// syntax of OTN::VERSION, used by OTNWriter and OTNStreamWriter
		namespace Current = V1;
		static_assert(VERSION == 1, "Detail::Current has to be the syntax namespace of OTN::VERSION");

		static const ReadSyntax* GetReadSyntax(uint8_t version) {
			switch (version) {
			case 1:  return &V1::READ_SYNTAX;
			default: return nullptr;
			}
		}

	}

#pragma endregion

#pragma region OTNObject

	// ======== OTNObject ========
	OTNObject::OTNObject(const std::string& name)
		: m_name(name) {
		DebugIsNameValid(name);
	}

	OTNObject::OTNObject(const OTNObject& other) noexcept
		: m_name(other.m_name),
		m_columnNames(other.m_columnNames),
		m_columnTypes(other.m_columnTypes),
		m_dataRows(other.m_dataRows),
		m_error(other.m_error),
		m_valid(other.m_valid),
		m_wasBuildFromT(other.m_wasBuildFromT),
		m_deducedColumnTypes(other.m_deducedColumnTypes) {
	}

	OTNObject& OTNObject::operator=(const OTNObject& other) noexcept {
		if (this == &other)
			return *this;

		m_name = other.m_name;
		m_columnNames = other.m_columnNames;
		m_columnTypes = other.m_columnTypes;
		m_dataRows = other.m_dataRows;
		m_error = other.m_error;
		m_valid = other.m_valid;
		m_wasBuildFromT = other.m_wasBuildFromT;
		m_deducedColumnTypes = other.m_deducedColumnTypes;
		m_columnIndexCache.clear();
		return *this;
	}

	OTNObject::OTNObject(OTNObject&& other) noexcept
		: m_name(std::move(other.m_name)),
		m_columnNames(std::move(other.m_columnNames)),
		m_columnTypes(std::move(other.m_columnTypes)),
		m_dataRows(std::move(other.m_dataRows)),
		m_error(std::move(other.m_error)),
		m_valid(other.m_valid),
		m_wasBuildFromT(other.m_wasBuildFromT),
		m_deducedColumnTypes(other.m_deducedColumnTypes),
		m_columnIndexCache(std::move(other.m_columnIndexCache)) {
	}

	OTNObject& OTNObject::operator=(OTNObject&& other) noexcept {
		m_name = std::move(other.m_name);
		m_columnNames = std::move(other.m_columnNames);
		m_columnTypes = std::move(other.m_columnTypes);
		m_dataRows = std::move(other.m_dataRows);
		m_error = std::move(other.m_error);
		m_valid = other.m_valid;
		m_wasBuildFromT = other.m_wasBuildFromT;
		m_deducedColumnTypes = other.m_deducedColumnTypes;
		m_columnIndexCache = std::move(other.m_columnIndexCache);
		return *this;
	}

	OTNObject& OTNObject::SetNamesList(const std::vector<std::string>& names) {
#ifndef NDEBUG
		if (!m_dataRows.empty()) {
			AddError(
				"SetNames must be called before AddData in object '" + m_name + "'!"
			);
			return *this;
		}
#endif
		m_columnIndexCache.clear();
		m_columnNames.clear();
		m_columnNames.reserve(names.size());
#ifndef NDEBUG
		for (const auto& n : names) {
			if (DebugIsNameValid(n))
				m_columnNames.emplace_back(n);
		}
		DebugValidateNamesDistinct();
#else
		m_columnNames = names;
#endif
		return *this;
	}

	OTNObject& OTNObject::SetTypesList(const std::vector<std::string>& types) {
#ifndef NDEBUG
		if (!m_dataRows.empty()) {
			AddError(
				"SetTypes must be called before AddData in object '" + m_name + "'!"
			);
			return *this;
		}
#endif

		std::vector<OTNTypeDesc> tmp;
		tmp.reserve(types.size());

		for (const std::string& t : types) {
			if (!AddSingleType(tmp, t))
				return *this;
		}

		m_columnTypes = std::move(tmp);
		return *this;
	}

	OTNObject& OTNObject::SetTypeDescList(const std::vector<OTNTypeDesc>& types) {
		m_columnTypes = types;
		return *this;
	}

	OTNObject& OTNObject::AddDataRowList(const OTNRow& values) {
#ifndef NDEBUG
		if (m_columnNames.empty()) {
			AddError("AddDataRow called before SetNames in object '" + m_name + "'!");
			return *this;
		}

		if (m_columnTypes.size() > m_columnNames.size()) {
			AddError("Cant have more types (count: "
				+ std::to_string(m_columnTypes.size())
				+ ") defined as names (count: "
				+ std::to_string(m_columnNames.size()) + ")!");
			return *this;
		}

		if (values.size() != m_columnNames.size()) {
			AddError("AddDataRow argument count (" + std::to_string(values.size()) +
				") does not match name count (" +
				std::to_string(m_columnNames.size()) +
				") in object '" + m_name + "'!");
			return *this;
		}
#endif

		OTNRow row;
		row.reserve(values.size());

		bool rowValid = true;
		for (auto& v : values) {
			AddSingleValueToRow(row, v, rowValid);
		}

		if (!rowValid)
			return *this;

		m_dataRows.emplace_back(std::move(row));

		if (m_dataRows.size() == 1 && !m_deducedColumnTypes) {
			if (!DeduceTypesFromRow(m_dataRows.back()))
				return *this;
		}

#ifndef NDEBUG
		DebugValidateDataTypes(m_dataRows.size() - 1);
#endif
		return *this;
	}

	OTNObject& OTNObject::ReserveDataRows(size_t amount) {
		m_dataRows.reserve(amount);
		return *this;
	}

	bool OTNObject::IsValid() const {
		return m_valid;
	}

	bool OTNObject::WasBuildFromT() const {
		return m_wasBuildFromT;
	}

	bool OTNObject::TryGetError(std::string& outError) {
		if (m_error.empty())
			return false;
		outError = m_error;
		return true;
	}

	std::string OTNObject::GetError() const {
		return m_error;
	}

	std::string OTNObject::GetObjectName() const {
		return m_name;
	}

	bool OTNObject::SetObjectName(const std::string& name) {
		bool result = DebugIsNameValid(name);
		if (result)
			m_name = name;
		return result;
	}

	size_t OTNObject::GetColumnCount() const {
		return m_columnNames.size();
	}

	size_t OTNObject::GetColumnCount(size_t rowIndex) const {
		if (rowIndex >= m_dataRows.size())
			throw std::out_of_range("OTNObject::GetColumnCount(rowIndex): row " + std::to_string(rowIndex) +
				" out of bounds (size=" + std::to_string(m_dataRows.size()) + ")");
		return m_dataRows[rowIndex].size();
	}

	size_t OTNObject::GetRowCount() const {
		return m_dataRows.size();
	}

	const std::vector<std::string>& OTNObject::GetColumnNames() const {
		return m_columnNames;
	}

	const std::vector<OTNTypeDesc>& OTNObject::GetColumnTypesDesc() const {
		return m_columnTypes;
	}

	const std::vector<OTNRow>& OTNObject::GetDataRows() const {
		return m_dataRows;
	}

	std::vector<std::string>& OTNObject::GetColumnNames() {
		return m_columnNames;
	}

	std::vector<OTNTypeDesc>& OTNObject::GetColumnTypesDesc() {
		return m_columnTypes;
	}

	std::vector<OTNRow>& OTNObject::GetDataRows() {
		return m_dataRows;
	}

	const OTNRow& OTNObject::GetRow(size_t index) const {
		if (index >= m_dataRows.size())
			throw std::out_of_range("OTNObject::GetRow: index " + std::to_string(index) +
				" out of bounds (size=" + std::to_string(m_dataRows.size()) + ")");
		return m_dataRows[index];
	}

	OTNRow& OTNObject::GetRow(size_t index) {
		if (index >= m_dataRows.size())
			throw std::out_of_range("OTNObject::GetRow: index " + std::to_string(index) +
				" out of bounds (size=" + std::to_string(m_dataRows.size()) + ")");
		return m_dataRows[index];
	}

	std::optional<size_t> OTNObject::GetColumnID(const std::string& name) const {
		auto it = m_columnIndexCache.find(name);
		if (it != m_columnIndexCache.end())
			return it->second;

		for (size_t i = 0; i < m_columnNames.size(); ++i) {
			if (m_columnNames[i] == name) {
				m_columnIndexCache[name] = i;
				return i;
			}
		}
		return std::nullopt;
	}

	bool OTNObject::IsAnyColumn(size_t column) const {
		return column < m_columnTypes.size() && m_columnTypes[column].baseType == OTNBaseType::ANY;
	}

	void OTNObject::AddError(const std::string& error) const {
		m_valid = false;
		if (!m_error.empty())
			m_error.push_back('\n');
		m_error += error;
	}

	void OTNObject::SetBuildFromT(bool value) {
		m_wasBuildFromT = value;
	}

	void OTNObject::SetNamesFromBuilder(std::vector<std::string>&& names) {
		if (!m_dataRows.empty()) {
			AddError(
				"SetNames must be called before AddData in object '"
				+ m_name + "'!");
			return;
		}

		m_columnIndexCache.clear();
		m_columnNames = std::move(names);
	}

	void OTNObject::AddRowInternal(OTNRow&& row) {
		m_dataRows.emplace_back(std::move(row));
	}

	bool OTNObject::AddSingleType(
		std::vector<OTNTypeDesc>& tempList,
		const std::string& t)
	{
		// Skip markers
		if (t.empty() || t == "-" || t == "_") {
			tempList.emplace_back(OTNBaseType::UNKNOWN, 0);
			return true;
		}

		std::optional<OTNTypeDesc> desc = Detail::ParseTypeString(t);
		if (!desc) {
			AddError("Missing base type in '" + t + "'!");
			return false;
		}

		// type could be an object name
		if (!desc->refObjectName.empty() && !DebugIsNameValid(desc->refObjectName)) {
			AddError("Type " + desc->refObjectName + " contains invalid chars!");
			return false;
		}

		tempList.push_back(std::move(*desc));
		return true;
	}

	bool OTNObject::DeduceTypesFromRow(const OTNRow& row) {
		if (m_columnNames.size() != row.size()) {
			AddError("Failed to deduce type for row");
			return false;
		}

		m_deducedColumnTypes = true;
		for (size_t i = 0; i < row.size(); i++) {
			OTNTypeDesc& actualType =
				(i >= m_columnTypes.size())
				? m_columnTypes.emplace_back(), m_columnTypes.back()
				: m_columnTypes[i];

			if (actualType.baseType != OTNBaseType::UNKNOWN)
				continue;

			const auto& value = row[i];
			OTNTypeDesc deducedType = DeduceTypeFromValue(value);

			actualType.baseType = deducedType.baseType;
			actualType.listDepth = deducedType.listDepth;

			// Set ref object if is ref now
			if (actualType.baseType == OTNBaseType::OBJECT) {
				actualType.refObjectName = deducedType.refObjectName;
			}
			else if (actualType.baseType == OTNBaseType::UNKNOWN) {
				m_deducedColumnTypes = false;
			}
		}

		return true;
	}

	bool OTNObject::DebugIsNameValid(const std::string& name) const {
#ifdef NDEBUG
		return true;
#else
		if (name.empty()) return false;

		// fast lookup table for invalid chars
		static const std::array<bool, 256> invalidCharTable = [] {
			std::array<bool, 256> table{};
			table[Syntax::ASSIGNMENT_CHAR] = true;
			table[Syntax::BLOCK_BEGIN_CHAR] = true;
			table[Syntax::BLOCK_END_CHAR] = true;
			table[Syntax::KEYWORD_ASSIGN_CHAR] = true;
			table[Syntax::KEYWORD_PREFIX_CHAR] = true;
			table[Syntax::LIST_BEGIN_CHAR] = true;
			table[Syntax::LIST_END_CHAR] = true;
			table[Syntax::SEPARATOR_CHAR] = true;
			table[Syntax::STATEMENT_TERMINATOR] = true;
			table[Syntax::TYPE_SEPARATOR_CHAR] = true;
			table['"'] = true;
			table['\''] = true;
			return table;
			}();

		unsigned char first = static_cast<unsigned char>(name[0]);
		if (first >= '0' && first <= '9')
			return false;

		for (const unsigned char* p = reinterpret_cast<const unsigned char*>(name.data());
			p < reinterpret_cast<const unsigned char*>(name.data() + name.size()); p++)
		{
			if (invalidCharTable[*p])
				return false;
		}

		return true;
#endif
	}

	bool OTNObject::DebugValidateNamesDistinct() {
#ifdef NDEBUG
		return true;
#else
		bool allUnique = true;
		std::unordered_map<std::string, int> nameCounts;

		for (const auto& name : m_columnNames) {
			++nameCounts[name];
		}

		std::vector<std::string> duplicates;
		for (const auto& [name, count] : nameCounts) {
			if (count > 1) {
				duplicates.push_back(name);
				allUnique = false;
			}
		}

		if (!duplicates.empty()) {
			std::string msg = "Duplicate names found: ";
			for (size_t i = 0; i < duplicates.size(); ++i) {
				msg += "'" + duplicates[i] + "'";
				if (i + 1 < duplicates.size()) {
					msg += ", ";
				}
			}
			AddError(msg + "!");
		}

		return allUnique;
#endif
	}

	bool OTNObject::DebugValidateDataTypes(size_t rowIndex) {
#ifdef NDEBUG
		return true;
#else
		if (m_dataRows.empty() || rowIndex >= m_dataRows.size())
			return true;

		bool valid = true;
		const OTNRow& row = m_dataRows[rowIndex];

		if (m_columnTypes.size() < row.size())
			m_columnTypes.resize(row.size());

		for (size_t col = 0; col < row.size(); ++col) {
			const OTNValue& val = row[col];
			OTNTypeDesc actual = DeduceTypeFromValue(val);
			OTNTypeDesc expected = m_columnTypes[col];

			if (expected.baseType == OTNBaseType::ANY)
				continue;

			if (actual.baseType == OTNBaseType::UNKNOWN ||
				expected.baseType == OTNBaseType::UNKNOWN)
				continue;

			// First row or UNKNOWN column defines the type
			if (expected.baseType == OTNBaseType::UNKNOWN) {
				if (rowIndex == 0)
					expected = actual;
				continue;
			}

			// treat Object refs as objects
			if (actual.baseType == OTNBaseType::OBJECT_REF)
				actual.baseType = OTNBaseType::OBJECT;

			if (expected.baseType == OTNBaseType::OBJECT_REF)
				expected.baseType = OTNBaseType::OBJECT;

			if ((expected.baseType != actual.baseType) ||
				expected.listDepth != actual.listDepth ||
				expected.refObjectName != actual.refObjectName) {

				AddError(
					std::string("Type mismatch at column '") +
					(col < m_columnNames.size() ? m_columnNames[col] : std::string("-")) +
					"' (row " + std::to_string(rowIndex) +
					"): expected '" + TypeDescToString(expected) +
					"', but found '" + TypeDescToString(actual) + "'!"
				);
				valid = false;
			}
		}

		return valid;
#endif
	}

	// ======== OTNObjectBuilder ========
	OTNObjectBuilder::OTNObjectBuilder(const OTNObject& obj)
		: m_objectName(obj.GetObjectName()), m_otnObjectFromT(false) {

		const std::vector<OTNRow>& rows = obj.GetDataRows();
		if (rows.empty()) {
			AddError("OTNObjectBuilder: Failed to create OTNObjectBuilder from OTNObject! Rows empty");
			return;
		}

		if (rows.size() > 1) {
			AddError("OTNObjectBuilder: Failed to create OTNObjectBuilder from OTNObject! Has more than 1 row");
			return;
		}

		m_data = rows[0];
	}

	void OTNObjectBuilder::SetObjectName(const std::string& name) {
		if (m_otnObjectFromT) {
			m_objectName = name;
		}
		else {
			// if building T from OTNValue check if object names are the same
			if (m_objectName != name) {
				AddError("OTNObjectBuilder: Error occurred while trying to build T from OTNObjectBuilder: object name mismatch (expected '"
					+ m_objectName + "', got '" + name + "')");
			}
		}
	}

	bool OTNObjectBuilder::IsBuildingOTNObject() const {
		return m_otnObjectFromT;
	}

	bool OTNObjectBuilder::IsValid() const {
		return m_valid;
	}

	std::string OTNObjectBuilder::GetError() const {
		return m_error;
	}

	OTNObject OTNObjectBuilder::ToOTNObject()&& {
		if (!m_otnObjectFromT) {
			throw std::runtime_error("OTNObjectBuilder: Build attempted OTNObject from T in invalid state");
		}

		OTNObject obj{ m_objectName };
		obj.SetBuildFromT(true);

		if (!m_valid) {
			obj.AddError(m_error);
			return obj;
		}

		if (IsDataOutOfSync()) {
			obj.AddError(
				"Object '" + m_objectName +
				"' has mismatched data and names ( size " +
				std::to_string(m_data.size()) + " != size " +
				std::to_string(m_dataNames.size()) + ")!"
			);
			return obj;
		}

		obj.SetNamesFromBuilder(std::move(m_dataNames));

		OTNRow row;
		row.reserve(m_data.size());

		for (auto& value : m_data) {
			row.emplace_back(std::move(value));
		}

		obj.AddRowInternal(std::move(row));

		return obj;
	}

	void OTNObjectBuilder::AddError(const std::string& error) {
		m_valid = false;
		if (!m_error.empty())
			m_error += "\n";
		m_error += error;
	}

	bool OTNObjectBuilder::AddName(std::string name) {
		if (!m_otnObjectFromT)
			return true;

#ifndef NDEBUG
		if (!IsNameUnique(name)) {
			AddError("OTNObjectBuilder: Name '" + name + "' is not unique in object '" + m_objectName + "'!");
			return false;
		}
#endif

		m_dataNames.push_back(std::move(name));
		return true;
	}

	bool OTNObjectBuilder::IsNameUnique(const std::string& name) const {
		return std::find(m_dataNames.begin(), m_dataNames.end(), name) == m_dataNames.end();
	}

	bool OTNObjectBuilder::IsDataOutOfSync() const {
		return m_data.size() != m_dataNames.size();
	}

#pragma endregion

#pragma region OTNWriter

	// ======== OTNWriter ========
	OTNWriter& OTNWriter::UseDefName(bool value) {
		m_useDefName = value;
		return *this;
	}

	OTNWriter& OTNWriter::UseDefType(bool value) {
		m_useDefType = value;
		return *this;
	}

	OTNWriter& OTNWriter::UseOptimizations(bool value) {
		m_useOptimizations = value;
		return *this;
	}

	OTNWriter& OTNWriter::UseDeduplicateRows(bool value) {
		m_useDeduplicateRows = value;
		return *this;
	}

	OTNWriter& OTNWriter::AppendObject(const OTNObject& object) {
#ifndef NDEBUG
		for (const auto& obj : m_objects) {
			if (obj.GetObjectName() == object.GetObjectName()) {
				AddError("Could not append object '" + object.GetObjectName() + "', an object with the name already exists!");
				return *this;
			}
		}
#endif 

		m_objects.push_back(object);
		return *this;
	}

	OTNWriter& OTNWriter::ClearObjects() {
		m_objects.clear();
		return *this;
	}

	OTNWriter& OTNWriter::ReserveObjects(size_t amount) {
		m_objects.reserve(amount);
		return *this;
	}

	bool OTNWriter::Save(const OTNFilePath& path) {
		if (!IsValid()) {
			AddError("Writer object is invalid!");
			return false;
		}

		OTNFilePath newPath;
		std::string error;
		if (!ValidateFilePath(path, CREATE_MISSING_DIR, newPath, error)) {
			AddError(error);
			AddError("File path '" + path.string() + "' was invalid!");
			return false;
		}

		if (!DebugValidateObjects()) {
			AddError("[Debug] Validation of objects failed!");
			return false;
		}

		if (!WriteToFile(newPath)) {
			AddError("Write to file failed!");
			return false;
		}

		if (!IsValid()) {
			AddError("Writer object is invalid!");
			return false;
		}

		return true;
	}

	bool OTNWriter::SaveToString(std::string& outText) {
		if (!IsValid()) {
			AddError("Writer object is invalid!");
			return false;
		}

		if (!DebugValidateObjects()) {
			AddError("[Debug] Validation of objects failed!");
			return false;
		}

		if (!WriteToString(outText)) {
			AddError("Write to string failed!");
			return false;
		}

		if (!IsValid()) {
			AddError("Writer object is invalid!");
			return false;
		}

		return true;
	}

	bool OTNWriter::GetUseDefName() const {
		return m_useDefName;
	}

	bool OTNWriter::GetUseDefType() const {
		return m_useDefType;
	}

	bool OTNWriter::GetUseOptimizations() const {
		return m_useOptimizations;
	}

	bool OTNWriter::GetDeduplicateRows() const {
		return m_useDeduplicateRows;
	}

	bool OTNWriter::IsValid() const {
		return m_valid;
	}

	bool OTNWriter::TryGetError(std::string& outError) const {
		if (m_valid)
			return false;
		outError = m_error;
		return true;
	}

	std::string OTNWriter::GetError() {
		return m_error;
	}

	size_t OTNWriter::SerializedObject::AddOrGetRow(const Row& row, bool deduplicateRows) {
		if (row.empty())
			return static_cast<size_t>(-1);

		if (!deduplicateRows) {
			size_t index = rows.size();
			rows.push_back(row);
			return index;
		}

		size_t hash = CreateRowHash(columnTypes, row);

		auto it = rowIndexByHash.find(hash);
		if (it != rowIndexByHash.end()) {
			size_t existingIndex = it->second;
			auto& currentRow = rows[existingIndex];

			if (currentRow.size() == row.size() &&
				currentRow == row) {
				return existingIndex;
			}
		}

		size_t index = rows.size();
		rowIndexByHash[hash] = index;
		rows.push_back(row);
		return index;
	}

	size_t OTNWriter::SerializedObject::CreateRowHash(const std::vector<OTNTypeDesc>& columnTypes, const Row& row) {
		size_t hash = 0;

#ifndef NDEBUG
		if (columnTypes.size() != row.size()) {
			assert(false && "CreateRowHash: columnTypes and row size mismatch");
		}
#endif

		for (size_t i = 0; i < row.size(); i++) {
			HashCombine(hash, HashValue(columnTypes[i], row[i]));
		}

		return hash;
	}

	size_t OTNWriter::SerializedObject::HashValue(const OTNTypeDesc& colType, const OTNValue& value) {
		size_t hash = 0;

		// Include value type
		OTNBaseType type = (value.type == OTNBaseType::LIST) ? OTNBaseType::LIST : colType.baseType;
		// 'any' columns can contain every type, use the type of the value itself
		if (type == OTNBaseType::ANY || type == OTNBaseType::UNKNOWN)
			type = value.type;
		HashCombine(hash, static_cast<size_t>(type));
		switch (type) {
		case OTNBaseType::INT:
			HashCombine(hash, std::hash<int>{}(std::get<int>(value.value)));
			break;

		case OTNBaseType::INT64:
			HashCombine(hash, std::hash<int64_t>{}(std::get<int64_t>(value.value)));
			break;

		case OTNBaseType::UINT64:
			HashCombine(hash, std::hash<uint64_t>{}(std::get<uint64_t>(value.value)));
			break;

		case OTNBaseType::FLOAT:
			HashCombine(hash, std::hash<float>{}(std::get<float>(value.value)));
			break;

		case OTNBaseType::DOUBLE:
			HashCombine(hash, std::hash<double>{}(std::get<double>(value.value)));
			break;

		case OTNBaseType::BOOL:
			HashCombine(hash, std::hash<bool>{}(std::get<bool>(value.value)));
			break;

		case OTNBaseType::STRING:
			HashCombine(hash, std::hash<std::string>{}(std::get<std::string>(value.value)));
			break;

		case OTNBaseType::LIST: {
			if (value.type != OTNBaseType::LIST) {
				HashCombine(hash, 0);
				break;
			}

			Detail::OTNArrayPtr arrayPtr = std::get<Detail::OTNArrayPtr>(value.value);
			if (!arrayPtr) {
				HashCombine(hash, 0);
				break;
			}

			HashCombine(hash, arrayPtr->values.size());
			HashCombine(hash, static_cast<size_t>(colType.listDepth));

			OTNTypeDesc elementType = colType;
			elementType.listDepth = (colType.listDepth > 0) ? colType.listDepth - 1 : 0;

			for (const auto& val : arrayPtr->values) {
				HashCombine(hash, HashValue(elementType, val));
			}
			break;
		}
		case OTNBaseType::OBJECT: {
			// size_t wird als uint64_t oder int gespeichert - beide abdecken
			if (value.type == OTNBaseType::INT) {
				HashCombine(hash, std::hash<int>{}(std::get<int>(value.value)));
			}
			else if (value.type == OTNBaseType::UINT64) {
				HashCombine(hash, std::hash<uint64_t>{}(std::get<uint64_t>(value.value)));
			}
			else {
				HashCombine(hash, 0);
#ifndef NDEBUG
				assert(false && "HashValue: type for Object was invalid");
#endif
			}
			break;
		}
		case OTNBaseType::UNKNOWN:
		default:
#ifndef NDEBUG
			assert(false && "HashValue: type for hashing was invalid");
#endif
			break;
		}

		return hash;
	}

	bool OTNWriter::DebugValidateObjects() {
#ifdef NDEBUG
		return true;
#else
		bool valid = true;
		for (const auto& obj : m_objects) {
			if (!obj.IsValid()) {
				if (valid) {
					AddError("Objects invalid!");
				}

				std::string msg = obj.GetError();
				// Remove trailing newline if present
				if (!msg.empty() && msg.back() == '\n') {
					msg.pop_back();
				}

				std::string objName = obj.GetObjectName();
				AddError("Object '" + objName + "' is invalid, Error: '" + msg + "'!");
				m_error += "\n";
				valid = false;
			}
		}

		return valid;
#endif
	}

	bool OTNWriter::WriteToFile(const OTNFilePath& path) {
		m_writerData.Reset();

		auto& stream = m_writerData.stream;
		stream.open(path, std::ios::binary);
		if (!stream.is_open())
			return false;

		bool ok = CreateWriteData(m_writerData) && WriteHeader() && WriteBody() && FlushBuffer(true);
		stream.close();
		return ok;
	}

	bool OTNWriter::WriteToString(std::string& outText) {
		m_writerData.Reset();
		outText.clear();

		if (!CreateWriteData(m_writerData) || !WriteHeader() || !WriteBody())
			return false;

		outText = std::move(m_writerData.buffer);
		m_writerData.buffer.clear();
		return true;
	}

	void OTNWriter::CountObjectType(const SerializedObject& obj, std::unordered_map<OTNBaseType, uint32_t>& typeUsage) {
		for (const auto& types : obj.columnTypes) {
			if (types.refObjectName.empty())
				typeUsage[types.baseType]++;
		}
	}

	OTNTypeDesc OTNWriter::DeduceColumnType(const OTNValue& value) {
		OTNTypeDesc result;
		const OTNValue* current = &value;

		while (current->type == OTNBaseType::LIST) {
			++result.listDepth;

			const auto& arr = std::get<Detail::OTNArrayPtr>(current->value);
			if (!arr || arr->values.empty())
				break;

			current = &arr->values.front();
		}

		result.baseType = current->type;

		if (current->type == OTNBaseType::OBJECT) {
			const auto& obj = std::get<Detail::OTNObjectPtr>(current->value);
			if (obj)
				result.refObjectName = obj->GetObjectName();
		}

		return result;
	}

	bool OTNWriter::CreateWriteData(WriterData& data) {
		if (data.created)
			data.Reset();

		// Convert objects
		for (OTNObject& obj : m_objects) {
			AddObject(data, obj);
		}

		// Count used types (read-only)
		for (const auto& [name, obj] : data.objects) {
			CountObjectType(obj, data.typeUsage);
		}

		data.created = true;

		if (m_useDefName) {
			if (!CreateDefName())
				return false;
		}

		if (m_useDefType) {
			if (!CreateDefType())
				return false;
		}

		return true;
	}

	std::vector<size_t> OTNWriter::AddObject(WriterData& data, OTNObject& object) {
		auto& objectMap = data.objects;

		// Create or get SerializedObject
		auto [it, inserted] = objectMap.try_emplace(object.GetObjectName());
		SerializedObject& serObj = it->second;

		if (inserted) {
			serObj.columnNames = object.GetColumnNames();
		}

		if (serObj.columnTypes.empty()) {
			serObj.columnTypes = object.GetColumnTypesDesc();
		}

		std::vector<size_t> indices;
		OTNValue outVal;
		// Convert rows
		for (const OTNRow& row : object.GetDataRows()) {
			SerializedObject::Row serRow;
			if (serObj.columnTypes.empty()) {
				serRow.reserve(row.size());
				serObj.columnTypes.reserve(row.size());
				for (const OTNValue& val : row) {
					serObj.columnTypes.push_back(DeduceColumnType(val));
				}
			}

			if (row.size() != serObj.columnTypes.size()) {
				assert(false && "AddObject: SerializedObject::Row and SerializedObject::ColumnType size mismatch");
			}

			size_t columnTypeIndex = 0;
			for (const OTNValue& val : row) {
#ifndef NDEBUG
				if (val.type == OTNBaseType::OBJECT_REF) {
					assert(false && "Writer cant save object ref");
				}
#endif

				ConvertToSerValue(data, outVal, serObj.columnTypes[columnTypeIndex], val);
				serRow.emplace_back(std::move(outVal));
				columnTypeIndex++;
			}

			indices.emplace_back(serObj.AddOrGetRow(serRow, m_useDeduplicateRows));
		}

		return indices;
	}

	void OTNWriter::ConvertToSerValue(WriterData& data, OTNValue& result, OTNTypeDesc& colType, const OTNValue& val) {
		if (val.type == OTNBaseType::LIST) {
			const Detail::OTNArrayPtr& arrayPtr = std::get<Detail::OTNArrayPtr>(val.value);
			Detail::OTNArrayPtr newArray = std::make_shared<Detail::OTNArray>();
			if (!arrayPtr || !newArray)
				return;

			newArray->values.reserve(arrayPtr->values.size());

			for (const auto& v : arrayPtr->values) {
				if (v.type == OTNBaseType::OBJECT) {
					OTNValue converted;
					ConvertToSerValue(data, converted, colType, v);
					if (converted.type == OTNBaseType::LIST) {
						const Detail::OTNArrayPtr& indices = std::get<Detail::OTNArrayPtr>(converted.value);
						if (indices) {
							for (auto& idx : indices->values) {
								newArray->values.emplace_back(std::move(idx));
							}
						}
					}
					else {
						newArray->values.emplace_back(std::move(converted));
					}
				}
				else {
					OTNValue converted;
					ConvertToSerValue(data, converted, colType, v);
					newArray->values.emplace_back(std::move(converted));
				}
			}

			result = OTNValue(std::move(newArray));
		}
		else if (val.type == OTNBaseType::OBJECT) {
			const Detail::OTNObjectPtr& objPtr = std::get<Detail::OTNObjectPtr>(val.value);
			if (!objPtr)
				return;

			std::vector<size_t> refIndex = AddObject(data, *objPtr);

			// if obj was build form T. can only have 1 data row and will be saved as Ref<obj>
			// Native otn objects could have multiple rows so will be save as Ref<obj>[]
			if (objPtr->WasBuildFromT()) {
				if (refIndex.empty())
					return;
				result = OTNValue(refIndex[0]);
			}
			else {
				Detail::OTNArrayPtr newArray = std::make_shared<Detail::OTNArray>();
				newArray->values.reserve(refIndex.size());
				for (const auto& idx : refIndex) {
					newArray->values.emplace_back(idx);
				}

				result = OTNValue(std::move(newArray));

				if (colType.listDepth == 0)
					colType.listDepth = 1;
			}
		}
		else {
			result = OTNValue(val);
		}
	}

	bool OTNWriter::CreateDefType() {
		auto& defType = m_writerData.fileHeader.defType;
		defType.Clear();
		uint32_t indexCount = 0;

		for (const auto& [type, used] : m_writerData.typeUsage) {
			if (type == OTNBaseType::OBJECT || type == OTNBaseType::LIST)
				continue;

			uint32_t length = OTNValueTypeCharLength(type);

			if (used > 1 && length * used > 9)
				defType.Add(indexCount++, std::string(OTNValueTypeToString(type)));
		}

		return true;
	}

	bool OTNWriter::CreateDefName() {
		auto& defName = m_writerData.fileHeader.defName;
		defName.Clear();

		// Count name usage
		std::unordered_map<std::string, uint32_t> nameUsage;

		for (const auto& [_, serObj] : m_writerData.objects) {
			for (const std::string& colName : serObj.columnNames) {
				++nameUsage[colName];
			}
		}

		uint32_t indexCount = 0;

		// skip if there are not enough names
		if (nameUsage.size() < 2)
			return true;

		for (const auto& [name, used] : nameUsage) {
			if (used > 1 && name.size() * used > 9) {
				defName.Add(indexCount++, name);
			}
		}

		return true;
	}

	bool OTNWriter::WriteHeader() {
		Detail::Current::WriteFileHeader(m_writerData.buffer, GetWriteOptions(), m_writerData.fileHeader);
		return true;
	}

	bool OTNWriter::WriteBody() {
		const Detail::WriteOptions options = GetWriteOptions();
		std::string& out = m_writerData.buffer;
		std::string error;

		Detail::Current::WriteObjectBlockBegin(out, options);

		for (const auto& [name, obj] : m_writerData.objects) {
			Detail::ObjectHeaderInfo info;
			info.name = name;
			info.rowCount = obj.rows.size();
			info.columnNames = obj.columnNames;
			info.columnTypes = obj.columnTypes;

			if (!Detail::Current::WriteObjectHeader(out, options, info, m_writerData.fileHeader, error)) {
				AddError("Could not save object '" + name + "': " + error + "!");
				return false;
			}

			for (const auto& row : obj.rows) {
				if (!Detail::Current::WriteRow(out, options, row, error)) {
					AddError("Could not save a row of object '" + name + "': " + error + "!");
					return false;
				}

				if (!FlushBuffer(false))
					return false;
			}
		}

		Detail::Current::WriteObjectBlockEnd(out, options);
		return true;
	}

	bool OTNWriter::FlushBuffer(bool force) {
		auto& stream = m_writerData.stream;
		auto& buffer = m_writerData.buffer;

		// writing into a string, keep everything in the buffer
		if (!stream.is_open())
			return true;

		if (!force && buffer.size() < FLUSH_SIZE)
			return true;

		stream.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		buffer.clear();
		if (!stream.good()) {
			AddError("Failed to write into the file!");
			return false;
		}
		return true;
	}

	Detail::WriteOptions OTNWriter::GetWriteOptions() const {
		Detail::WriteOptions options;
		options.compact = m_useOptimizations;
		return options;
	}

	void OTNWriter::AddError(const std::string& error) {
		if (!m_error.empty())
			m_error += "\n";
		m_error += error;
		m_valid = false;
	}

#pragma endregion

#pragma region OTNReader

	// ======== OTNReader ========
	bool OTNReader::ReadFile(const OTNFilePath& path) {
		if (!IsValid()) {
			AddError("Reader object is invalid!");
			return false;
		}

		OTNFilePath newPath;
		std::string error;
		if (!ValidateFilePath(path, !CREATE_MISSING_DIR, newPath, error)) {
			AddError(error);
			AddError("File path was invalid!");
			return false;
		}

		// Slurp the whole file — avoids char-by-char istream overhead in tokenizer
		std::ifstream f(newPath, std::ios::binary | std::ios::ate);
		if (!f.is_open()) {
			AddError("Could not open file!");
			return false;
		}
		std::string buf(static_cast<size_t>(f.tellg()), '\0');
		f.seekg(0);
		f.read(buf.data(), static_cast<std::streamsize>(buf.size()));
		f.close();

		m_readerData.Reset();
		if (!ReadData(buf, m_readerData)) {
			AddError("Data could not be read!");
			return false;
		}

		return true;
	}

	bool OTNReader::ReadString(const std::string& fileString) {
		if (!IsValid()) {
			AddError("Reader object is invalid!");
			return false;
		}

		m_readerData.Reset();

		if (!ReadData(fileString, m_readerData)) {
			AddError("Data could not be read!");
			return false;
		}

		return true;
	}

	uint8_t OTNReader::GetVersion() const {
		return m_readerData.version;
	}

	std::optional<OTNObject> OTNReader::TryGetObject(const std::string& objName) const {
		const auto& objMap = m_readerData.objects;

		auto it = objMap.find(objName);
		if (it == objMap.end())
			return std::nullopt;

		return it->second;
	}

	const std::unordered_map<std::string, OTNObject>& OTNReader::GetObjects() const {
		return m_readerData.objects;
	}

	std::unordered_map<std::string, OTNObject>& OTNReader::GetObjects() {
		return m_readerData.objects;
	}

	bool OTNReader::IsValid() const {
		return m_valid;
	}

	std::string OTNReader::GetError() const {
		return m_error;
	}

	bool OTNReader::TryGetError(std::string& outError) const {
		if (IsValid())
			return false;

		outError = m_error;
		return true;
	}

	bool OTNReader::ReadData(std::string_view src, ReaderData& data) {
		Detail::CharSource source;
		source.OpenMemory(src);
		Detail::Tokenizer tok(source);

		std::optional<uint8_t> version = Detail::ReadVersion(tok);
		if (!version) {
			AddError("Could not determine file version!");
			AddError(tok.GetError());
			return false;
		}
		data.version = *version;

		const Detail::ReadSyntax* syntax = Detail::GetReadSyntax(*version);
		if (!syntax) {
			AddError("Unsupported OTN version: " + std::to_string(*version) + "!");
			return false;
		}

		std::optional<Detail::FileHeaderInfo> header = syntax->readFileHeader(tok, *version);
		while (header && syntax->hasMoreObjects(tok)) {
			std::optional<Detail::ObjectHeaderInfo> info = syntax->readObjectHeader(tok, *header);
			if (!info)
				break;

			OTNObject obj{ info->name };
			obj.SetNamesList(info->columnNames);
			obj.SetTypeDescList(info->columnTypes);
			obj.ReserveDataRows(info->rowCount);

			bool rowsRead = true;
			for (size_t r = 0; r < info->rowCount; ++r) {
				std::optional<OTNRow> row = syntax->readRow(tok, info->columnTypes);
				if (!row) {
					rowsRead = false;
					break;
				}
				obj.AddDataRowList(*row);
			}

			if (!rowsRead)
				break;

			if (!obj.IsValid()) {
				AddError("Error while trying to read object '" + info->name + "' error: " + obj.GetError());
				return false;
			}

			auto [it, inserted] = data.objects.emplace(info->name, std::move(obj));
			if (!inserted) {
				AddError("Object '" + info->name + "' already defined");
				return false;
			}
		}

		if (header && tok.IsValid())
			syntax->readObjectBlockEnd(tok);

		if (!tok.IsValid()) {
			AddError("Failed to read file!");
			AddError(tok.GetError());
			return false;
		}

		return ResolveObjectRefs(data);
	}

	bool OTNReader::ResolveObjectRefs(ReaderData& data) {
		RefStack stack;
		for (auto& [objectName, object] : data.objects) {
			for (OTNRow& row : object.GetDataRows()) {
				for (OTNValue& value : row) {
					if (!ResolveValue(data, objectName, value, stack))
						return false;
				}
			}
		}
		return true;
	}

	bool OTNReader::ResolveValue(ReaderData& data, const std::string& ownerObjectName, OTNValue& value, RefStack& stack) {
		if (value.type == OTNBaseType::LIST) {
			const Detail::OTNArrayPtr& list = std::get<Detail::OTNArrayPtr>(value.value);
			if (!list)
				return true;

			for (OTNValue& elem : list->values) {
				if (!ResolveValue(data, ownerObjectName, elem, stack))
					return false;
			}
			return true;
		}

		if (value.type != OTNBaseType::OBJECT_REF)
			return true;

		const Detail::OTNObjectRef ref = std::get<Detail::OTNObjectRef>(value.value);

		auto it = data.objects.find(ref.refObjectName);
		if (it == data.objects.end()) {
			AddError(
				"Object reference could not be resolved: no object named '" +
				ref.refObjectName + "' exists (referenced from object '" +
				ownerObjectName + "')"
			);
			return false;
		}

		const OTNObject& target = it->second;
		const auto& targetRows = target.GetDataRows();
		if (ref.index >= targetRows.size()) {
			AddError(
				"Object reference index out of bounds: index " +
				std::to_string(ref.index) +
				" for object '" + ref.refObjectName +
				"' (referenced from object '" + ownerObjectName + "')"
			);
			return false;
		}

		for (const auto& [name, index] : stack) {
			if (index == ref.index && name == ref.refObjectName) {
				AddError("Cyclic object reference to row " + std::to_string(ref.index) +
					" of object '" + ref.refObjectName + "'");
				return false;
			}
		}

		// resolve the references of the referenced row (recursive) on a copy
		OTNRow row = targetRows[ref.index];
		stack.emplace_back(ref.refObjectName, ref.index);
		for (OTNValue& elem : row) {
			if (!ResolveValue(data, ref.refObjectName, elem, stack))
				return false;
		}
		stack.pop_back();

		Detail::OTNObjectPtr resolved = std::make_shared<OTNObject>(ref.refObjectName);
		resolved->SetNamesList(target.GetColumnNames());
		resolved->SetTypeDescList(target.GetColumnTypesDesc());
		resolved->AddDataRowList(row);

		value = OTNValue(resolved);
		return true;
	}

	void OTNReader::AddError(const std::string& error) {
		if (!m_error.empty())
			m_error += "\n";
		m_error += error;
		m_valid = false;
	}

#pragma endregion

#pragma region OTNStreamWriter

	// true if a value of type 'type' can be written into a column of type 'column'
	static bool IsCompatibleTypeDesc(const OTNTypeDesc& column, const OTNTypeDesc& type) {
		// deduced from an empty list, the element type is unknown
		if (type.baseType == OTNBaseType::UNKNOWN)
			return type.listDepth > 0 && type.listDepth <= column.listDepth;

		return column.baseType == type.baseType && column.listDepth == type.listDepth &&
			(column.baseType != OTNBaseType::OBJECT || column.refObjectName == type.refObjectName);
	}

	// Column types of an object, objects build from T have no types -> deduce them from the first row like OTNWriter
	static std::vector<OTNTypeDesc> GetStreamColumnTypes(const OTNObject& obj) {
		std::vector<OTNTypeDesc> types = obj.GetColumnTypesDesc();
		if (types.empty() && obj.GetRowCount() > 0) {
			for (const OTNValue& val : obj.GetRow(0))
				types.push_back(DeduceTypeFromValue(val));

			// empty lists have no element type, try the other rows
			for (size_t c = 0; c < types.size(); ++c) {
				for (size_t r = 1; r < obj.GetRowCount() && types[c].baseType == OTNBaseType::UNKNOWN; ++r) {
					const OTNRow& row = obj.GetRow(r);
					if (c < row.size())
						types[c] = DeduceTypeFromValue(row[c]);
				}
			}
		}

		// Native objects (not build from T) are referenced as list of rows (Ref<X>[]), like OTNWriter does
		for (size_t c = 0; c < types.size(); ++c) {
			OTNTypeDesc& ct = types[c];
			if (ct.baseType != OTNBaseType::OBJECT || ct.listDepth != 0)
				continue;

			for (const OTNRow& row : obj.GetDataRows()) {
				if (c >= row.size() || row[c].type != OTNBaseType::OBJECT)
					continue;

				const auto& ptr = std::get<Detail::OTNObjectPtr>(row[c].value);
				if (ptr && !ptr->WasBuildFromT()) {
					ct.listDepth = 1;
					break;
				}
			}
		}
		return types;
	}

	OTNStreamWriter::~OTNStreamWriter() {
		if (m_fileOpen)
			Close();
		DiscardRowFile();
	}

	void OTNStreamWriter::ResetFileState() {
		DiscardRowFile();
		m_valid = true;
		m_error.clear();
		m_objectOpen = false;
		m_rowFileSize = 0;
		m_blocks.clear();
		m_blockIndex.clear();
		m_currentBlock = NO_BLOCK;
		m_objPtrToIndex.clear();
		m_objectsInProgress.clear();
	}

	bool OTNStreamWriter::Open(const OTNFilePath& path) {
		// finish a previous file, every file starts with a fresh state
		if (m_fileOpen)
			Close();
		ResetFileState();

		OTNFilePath finalPath;
		std::string pathError;
		if (!ValidateFilePath(path, CREATE_MISSING_DIR, finalPath, pathError)) {
			AddError(pathError);
			return false;
		}

		m_file.open(finalPath, std::ios::binary | std::ios::out | std::ios::trunc);
		if (!m_file.is_open()) {
			AddError("OTNStreamWriter: failed to open '" + finalPath.string() + "'");
			return false;
		}
		m_filePath = finalPath;
		m_rowFilePath = finalPath;
		m_rowFilePath.replace_extension(OTN::TEMP_FILE_EXTENSION);

		// rows of all objects are collected in a temporary file, the target file is written on Close
		m_rowFile.open(m_rowFilePath, std::ios::binary | std::ios::out | std::ios::trunc);
		if (!m_rowFile.is_open()) {
			AddError("OTNStreamWriter: failed to create temporary file '" + m_rowFilePath.string() + "'");
			m_file.close();
			return false;
		}

		m_fileOpen = true;
		return true;
	}

	bool OTNStreamWriter::BeginObject(
		const std::string& name,
		const std::vector<std::string>& columnNames,
		const std::vector<std::string>& columnTypes)
	{
		if (!m_valid)
			return false;

		if (!m_fileOpen) {
			AddError("OTNStreamWriter: file not open");
			return false;
		}

		if (m_objectOpen) {
			AddError("OTNStreamWriter: previous object not closed");
			return false;
		}

		if (columnNames.size() != columnTypes.size()) {
			AddError("OTNStreamWriter: column name / type count mismatch in '" + name + "'");
			return false;
		}

		// Parse type descriptors from strings
		std::vector<OTNTypeDesc> types;
		types.reserve(columnTypes.size());
		for (const auto& t : columnTypes) {
			std::optional<OTNTypeDesc> desc = Detail::ParseTypeString(t);
			if (!desc) {
				AddError("OTNStreamWriter: invalid column type '" + t + "' in '" + name + "'");
				return false;
			}
			types.push_back(std::move(*desc));
		}

		size_t block = NO_BLOCK;
		if (!GetBlock(name, columnNames, types, block))
			return false;

		m_currentBlock = block;
		m_objectOpen = true;
		return true;
	}

	bool OTNStreamWriter::CheckRowPreconditions(size_t argCount) {
		if (!m_valid)
			return false;

		if (!m_objectOpen) {
			AddError("OTNStreamWriter: no object open");
			return false;
		}

		size_t columnCount = m_blocks[m_currentBlock].header.columnNames.size();
		if (argCount != columnCount) {
			AddError("OTNStreamWriter: row argument count (" + std::to_string(argCount) +
				") does not match column count (" + std::to_string(columnCount) + ")");
			return false;
		}

		return true;
	}

	bool OTNStreamWriter::WriteRowList(const OTNRow& row) {
		if (!CheckRowPreconditions(row.size()))
			return false;

		return WriteRowInternal(row);
	}

	bool OTNStreamWriter::WriteRowInternal(const OTNRow& row) {
		const size_t block = m_currentBlock;
		// copy, writing sub-objects can add blocks
		const std::vector<OTNTypeDesc> colTypes = m_blocks[block].header.columnTypes;

		OTNRow converted;
		if (!ConvertRow(row, colTypes, converted))
			return false;

		std::string rowStr;
		std::string error;
		if (!Detail::Current::WriteRow(rowStr, m_writeOptions, converted, error)) {
			AddError("OTNStreamWriter: invalid row in '" + m_blocks[block].header.name + "': " + error);
			return false;
		}

		return AppendRows(block, rowStr, 1);
	}

	bool OTNStreamWriter::EndObject() {
		if (!m_objectOpen) {
			AddError("OTNStreamWriter: no object open to end");
			return false;
		}

		m_objectOpen = false;
		m_currentBlock = NO_BLOCK;
		return true;
	}

	bool OTNStreamWriter::GetBlock(const std::string& name, const std::vector<std::string>& colNames,
		const std::vector<OTNTypeDesc>& colTypes, size_t& outBlock)
	{
		auto it = m_blockIndex.find(name);
		if (it != m_blockIndex.end()) {
			// rows are appended to the existing object, so the columns have to be the same
			const Detail::ObjectHeaderInfo& header = m_blocks[it->second].header;
			bool same = header.columnNames == colNames && header.columnTypes.size() == colTypes.size();
			for (size_t i = 0; same && i < colTypes.size(); ++i)
				same = IsCompatibleTypeDesc(header.columnTypes[i], colTypes[i]);

			if (!same) {
				AddError("OTNStreamWriter: object '" + name + "' was already written with other column names or types");
				return false;
			}

			outBlock = it->second;
			return true;
		}

		ObjectBlock block;
		block.header.name = name;
		block.header.columnNames = colNames;
		block.header.columnTypes = colTypes;

		// the header is written on Close, check now that it can be written
		std::string headerStr;
		std::string error;
		if (!Detail::Current::WriteObjectHeader(headerStr, m_writeOptions, block.header, m_fileHeader, error)) {
			AddError("OTNStreamWriter: " + error);
			return false;
		}

		outBlock = m_blocks.size();
		m_blockIndex.emplace(name, outBlock);
		m_blocks.push_back(std::move(block));
		return true;
	}

	bool OTNStreamWriter::AppendRows(size_t block, const std::string& rows, size_t rowCount) {
		if (rowCount == 0)
			return true;

		m_rowFile.write(rows.data(), static_cast<std::streamsize>(rows.size()));
		if (!m_rowFile.good()) {
			AddError("OTNStreamWriter: failed to write data rows to temporary file");
			return false;
		}

		ObjectBlock& b = m_blocks[block];
		// rows written directly after the last range of the same object extend it
		if (!b.ranges.empty() && b.ranges.back().offset + b.ranges.back().length == m_rowFileSize)
			b.ranges.back().length += rows.size();
		else
			b.ranges.push_back({ m_rowFileSize, rows.size() });

		m_rowFileSize += rows.size();
		b.header.rowCount += rowCount;
		return true;
	}

	size_t OTNStreamWriter::GetWrittenRowCount(const std::string& objectName) const {
		auto it = m_blockIndex.find(objectName);
		return (it != m_blockIndex.end()) ? m_blocks[it->second].header.rowCount : 0;
	}

	std::optional<size_t> OTNStreamWriter::FindWrittenIndex(const Detail::OTNObjectPtr& ptr) const {
		if (!ptr)
			return std::nullopt;

		auto it = m_objPtrToIndex.find(ptr.get());
		if (it == m_objPtrToIndex.end())
			return std::nullopt;

		// address could belong to a new object if the old one was destroyed
		if (it->second.owner.lock() != ptr)
			return std::nullopt;

		return it->second.index;
	}

	void OTNStreamWriter::AppendObjectIndices(const Detail::OTNObjectPtr& ptr, size_t first, std::vector<size_t>& outIndices) {
		// same as OTNWriter: objects build from T have one row, native objects are referenced row by row
		if (ptr->WasBuildFromT()) {
			outIndices.push_back(first);
			return;
		}

		for (size_t i = 0; i < ptr->GetRowCount(); ++i)
			outIndices.push_back(first + i);
	}

	bool OTNStreamWriter::WriteSubObject(const Detail::OTNObjectPtr& ptr, std::vector<size_t>& outIndices) {
		if (!ptr)
			return true;

		if (std::optional<size_t> first = FindWrittenIndex(ptr)) {
			AppendObjectIndices(ptr, *first, outIndices);
			return true;
		}

		if (m_objectsInProgress.count(ptr.get())) {
			AddError("OTNStreamWriter: object '" + ptr->GetObjectName() + "' contains itself");
			return false;
		}

		m_objectsInProgress.insert(ptr.get());
		size_t first = 0;
		bool ok = WriteObjectRows(*ptr, first);
		m_objectsInProgress.erase(ptr.get());
		if (!ok)
			return false;

		m_objPtrToIndex[ptr.get()] = { ptr, first };
		AppendObjectIndices(ptr, first, outIndices);
		return true;
	}

	bool OTNStreamWriter::WriteObjectRows(const OTNObject& obj, size_t& outFirstRow) {
		const std::string& name = obj.GetObjectName();
		if (!obj.IsValid()) {
			AddError("OTNStreamWriter: object '" + name + "' is invalid: " + obj.GetError());
			return false;
		}

		const std::vector<std::string>& colNames = obj.GetColumnNames();
		const std::vector<OTNTypeDesc> colTypes = GetStreamColumnTypes(obj);
		if (colTypes.size() != colNames.size()) {
			AddError("OTNStreamWriter: object '" + name +
				"' has no type for every column (call SetTypes or add a data row)");
			return false;
		}

		// sub-objects are written while converting, the rows of this object are collected first
		// and appended at once, so they stay together and get consecutive indices
		const auto& rows = obj.GetDataRows();
		std::string rowsStr;
		OTNRow converted;
		for (size_t r = 0; r < rows.size(); ++r) {
			if (!ConvertRow(rows[r], colTypes, converted))
				return false;

			std::string error;
			if (!Detail::Current::WriteRow(rowsStr, m_writeOptions, converted, error)) {
				AddError("OTNStreamWriter: invalid row " + std::to_string(r) + " of '" + name + "': " + error);
				return false;
			}
		}

		size_t block = NO_BLOCK;
		if (!GetBlock(name, colNames, colTypes, block))
			return false;

		outFirstRow = m_blocks[block].header.rowCount;
		return AppendRows(block, rowsStr, rows.size());
	}

	bool OTNStreamWriter::ConvertRow(const OTNRow& row, const std::vector<OTNTypeDesc>& colTypes, OTNRow& outRow) {
		outRow.clear();
		outRow.reserve(row.size());
		for (size_t c = 0; c < row.size(); ++c) {
			const OTNTypeDesc colType = (c < colTypes.size()) ? colTypes[c] : OTNTypeDesc{};
			OTNValue value;
			if (!ConvertValue(row[c], colType, value))
				return false;
			outRow.push_back(std::move(value));
		}
		return true;
	}

	//   OTNObjectPtr -> int (row index in the target object) or [int, ...] for list columns
	//   LIST<OBJECT> -> LIST<int>
	//   Everything else is returned unchanged.

	bool OTNStreamWriter::ConvertValue(const OTNValue& val, const OTNTypeDesc& colType, OTNValue& outValue) {
		if (val.type == OTNBaseType::OBJECT) {
			std::vector<size_t> indices;
			if (!WriteSubObject(std::get<Detail::OTNObjectPtr>(val.value), indices))
				return false;

			if (colType.listDepth == 0) {
				uint64_t idx = indices.empty() ? 0 : static_cast<uint64_t>(indices.front());
				outValue = OTNValue(OTNValueVariant(idx));
				return true;
			}

			auto newArr = std::make_shared<Detail::OTNArray>();
			newArr->values.reserve(indices.size());
			for (size_t idx : indices)
				newArr->values.emplace_back(OTNValueVariant(static_cast<uint64_t>(idx)));

			outValue = OTNValue(OTNValueVariant(newArr));
			return true;
		}

		// only lists of object columns can contain objects
		const auto* arr = (val.type == OTNBaseType::LIST) ? &std::get<Detail::OTNArrayPtr>(val.value) : nullptr;
		if (!arr || !*arr || colType.baseType != OTNBaseType::OBJECT) {
			outValue = val;
			return true;
		}

		OTNTypeDesc elemType = colType;
		if (elemType.listDepth > 0)
			elemType.listDepth--;

		// Convert list<OTNObjectPtr> → list<int>
		auto newArr = std::make_shared<Detail::OTNArray>();
		newArr->values.reserve((*arr)->values.size());
		for (const OTNValue& elem : (*arr)->values) {
			if (elem.type == OTNBaseType::OBJECT) {
				std::vector<size_t> indices;
				if (!WriteSubObject(std::get<Detail::OTNObjectPtr>(elem.value), indices))
					return false;
				for (size_t idx : indices)
					newArr->values.emplace_back(OTNValueVariant(static_cast<uint64_t>(idx)));
				continue;
			}

			OTNValue converted;
			if (!ConvertValue(elem, elemType, converted))
				return false;
			newArr->values.push_back(std::move(converted));
		}

		outValue = OTNValue(OTNValueVariant(newArr));
		return true;
	}

	bool OTNStreamWriter::AppendObject(const OTNObject& obj) {
		if (!m_valid)
			return false;

		if (!m_fileOpen) {
			AddError("OTNStreamWriter: file not open");
			return false;
		}

		if (m_objectOpen) {
			AddError("OTNStreamWriter: cannot AppendObject while BeginObject is active");
			return false;
		}

		m_objectsInProgress.insert(&obj);
		size_t first = 0;
		bool ok = WriteObjectRows(obj, first);
		m_objectsInProgress.erase(&obj);
		return ok;
	}

	bool OTNStreamWriter::WriteFile() {
		m_rowFile.close();
		if (m_rowFile.fail()) {
			AddError("OTNStreamWriter: failed to finish temporary file '" + m_rowFilePath.string() + "'");
			return false;
		}

		std::string text;
		Detail::Current::WriteFileHeader(text, m_writeOptions, m_fileHeader);

		if (!m_blocks.empty()) {
			std::ifstream in(m_rowFilePath, std::ios::binary);
			if (!in.is_open()) {
				AddError("OTNStreamWriter: failed to open temporary file '" + m_rowFilePath.string() + "'");
				return false;
			}

			Detail::Current::WriteObjectBlockBegin(text, m_writeOptions);
			for (const ObjectBlock& block : m_blocks) {
				std::string error;
				if (!Detail::Current::WriteObjectHeader(text, m_writeOptions, block.header, m_fileHeader, error)) {
					AddError("OTNStreamWriter: " + error);
					return false;
				}

				m_file.write(text.data(), static_cast<std::streamsize>(text.size()));
				text.clear();
				if (!CopyRanges(block, in))
					return false;
			}
			Detail::Current::WriteObjectBlockEnd(text, m_writeOptions);
		}

		m_file.write(text.data(), static_cast<std::streamsize>(text.size()));
		m_file.flush();
		if (!m_file.good()) {
			AddError("OTNStreamWriter: failed to write '" + m_filePath.string() + "'");
			return false;
		}
		return true;
	}

	bool OTNStreamWriter::CopyRanges(const ObjectBlock& block, std::ifstream& in) {
		std::vector<char> buffer(COPY_BUFFER_SIZE);
		for (const RowRange& range : block.ranges) {
			in.clear();
			in.seekg(static_cast<std::streamoff>(range.offset));

			uint64_t remaining = range.length;
			while (remaining > 0) {
				const std::streamsize count = static_cast<std::streamsize>(std::min<uint64_t>(remaining, buffer.size()));
				in.read(buffer.data(), count);
				if (in.gcount() != count) {
					AddError("OTNStreamWriter: failed to read temporary file '" + m_rowFilePath.string() + "'");
					return false;
				}

				m_file.write(buffer.data(), count);
				if (!m_file.good()) {
					AddError("OTNStreamWriter: failed to copy rows of '" + block.header.name + "' into the file");
					return false;
				}
				remaining -= static_cast<uint64_t>(count);
			}
		}
		return true;
	}

	void OTNStreamWriter::DiscardRowFile() {
		if (m_rowFile.is_open())
			m_rowFile.close();

		if (m_rowFilePath.empty())
			return;

		std::error_code ec;
		std::filesystem::remove(m_rowFilePath, ec);
	}

	bool OTNStreamWriter::Close() {
		if (!m_fileOpen)
			return true;

		if (m_objectOpen)
			EndObject();

		bool ok = WriteFile();
		m_file.close();
		DiscardRowFile();
		m_fileOpen = false;
		return ok && m_valid;
	}

	bool OTNStreamWriter::IsValid()  const { return m_valid; }
	std::string OTNStreamWriter::GetError() const { return m_error; }
	bool OTNStreamWriter::TryGetError(std::string& outError) const {
		if (m_valid) return false;
		outError = m_error;
		return true;
	}
	void OTNStreamWriter::AddError(const std::string& error) {
		if (!m_error.empty()) m_error += '\n';
		m_error += error;
		m_valid = false;
	}

#pragma endregion

#pragma region OTNStreamReader

	OTNStreamReader::OTNStreamReader() = default;
	OTNStreamReader::~OTNStreamReader() = default;

	bool OTNStreamReader::Open(const OTNFilePath& path) {
		// every file starts with a fresh state
		m_tok.reset();
		m_source.reset();
		m_syntax = nullptr;
		m_fileHeader = {};
		m_path.clear();
		m_fileSize = 0;
		m_fileTime = {};
		m_objects.clear();
		m_objectIndex.clear();
		m_valid = true;
		m_error.clear();
		m_currentObject = NO_OBJECT;
		m_currentRow = 0;
		m_currentName.clear();
		m_columnNames.clear();
		m_columnTypes.clear();
		m_remainingRows = 0;

		OTNFilePath finalPath;
		std::string pathError;
		if (!ValidateFilePath(path, !CREATE_MISSING_DIR, finalPath, pathError)) {
			AddError(pathError);
			return false;
		}

		m_path = finalPath;
		if (!Scan()) {
			AddError("OTNStreamReader: failed to scan '" + m_path.string() + "'");
			return false;
		}
		return true;
	}

	bool OTNStreamReader::ReadFileStamp(uintmax_t& outSize, std::filesystem::file_time_type& outTime) const {
		std::error_code ec;
		outSize = std::filesystem::file_size(m_path, ec);
		if (ec)
			return false;

		outTime = std::filesystem::last_write_time(m_path, ec);
		return !ec;
	}

	// Reads the header and builds the object index: names, columns, row counts and row checkpoints.
	bool OTNStreamReader::Scan() {
		// take the stamp before reading, a change during the scan is detected by the next check
		if (!ReadFileStamp(m_fileSize, m_fileTime)) {
			AddError("OTNStreamReader: cannot access '" + m_path.string() + "'");
			return false;
		}

		m_tok.reset();
		m_source = std::make_unique<Detail::CharSource>();
		if (!m_source->OpenFile(m_path)) {
			AddError("OTNStreamReader: cannot open '" + m_path.string() + "'");
			return false;
		}
		m_tok = std::make_unique<Detail::Tokenizer>(*m_source);

		m_objects.clear();
		m_objectIndex.clear();
		m_syntax = nullptr;
		m_fileHeader = {};

		std::optional<uint8_t> version = Detail::ReadVersion(*m_tok);
		if (!version)
			return TokenizerFailed();
		m_fileHeader.version = *version;

		m_syntax = Detail::GetReadSyntax(*version);
		if (!m_syntax) {
			AddError("OTNStreamReader: unsupported OTN version " + std::to_string(*version));
			return false;
		}

		std::optional<Detail::FileHeaderInfo> header = m_syntax->readFileHeader(*m_tok, *version);
		if (!header)
			return TokenizerFailed();
		m_fileHeader = std::move(*header);

		while (m_syntax->hasMoreObjects(*m_tok)) {
			std::optional<Detail::ObjectHeaderInfo> objectHeader = m_syntax->readObjectHeader(*m_tok, m_fileHeader);
			if (!objectHeader)
				return TokenizerFailed();

			if (m_objectIndex.count(objectHeader->name)) {
				AddError("OTNStreamReader: object '" + objectHeader->name + "' already defined");
				return false;
			}

			ObjectInfo info;
			info.header = std::move(*objectHeader);
			info.rowCheckpoints.push_back(m_tok->Tell());
			for (size_t row = 0; row < info.header.rowCount; ++row) {
				if (row > 0 && row % ROW_CHECKPOINT_INTERVAL == 0)
					info.rowCheckpoints.push_back(m_tok->Tell());

				if (!m_syntax->skipRow(*m_tok))
					return TokenizerFailed();
			}

			m_objectIndex[info.header.name] = m_objects.size();
			m_objects.push_back(std::move(info));
		}

		if (!m_tok->IsValid() || !m_syntax->readObjectBlockEnd(*m_tok))
			return TokenizerFailed();

		return m_valid;
	}

	// Checks if the file was changed since the last scan. If so it is scanned again
	// and the sequential read position (object name + row) is restored.
	bool OTNStreamReader::EnsureUpToDate() {
		if (!m_valid)
			return false;

		if (!m_tok) {
			AddError("OTNStreamReader: no file open");
			return false;
		}

		uintmax_t size = 0;
		std::filesystem::file_time_type time{};
		if (!ReadFileStamp(size, time)) {
			AddError("OTNStreamReader: cannot access '" + m_path.string() + "'");
			return false;
		}

		if (size == m_fileSize && time == m_fileTime)
			return true;

		const bool hadObject = m_currentObject != NO_OBJECT;
		const std::string objectName = m_currentName;
		const size_t row = m_currentRow;

		if (!Scan()) {
			AddError("OTNStreamReader: failed to scan the changed file '" + m_path.string() + "'");
			return false;
		}

		if (!hadObject) {
			m_currentObject = NO_OBJECT;
			return true;
		}

		auto it = m_objectIndex.find(objectName);
		if (it == m_objectIndex.end()) {
			AddError("OTNStreamReader: file was changed, current object '" + objectName + "' no longer exists");
			return false;
		}

		const ObjectInfo& info = m_objects[it->second];
		if (row > info.header.rowCount) {
			AddError("OTNStreamReader: file was changed, object '" + objectName + "' has less rows than already read");
			return false;
		}

		SetCurrentObject(it->second, row);
		return SeekToRow(info, row);
	}

	const OTNStreamReader::ObjectInfo* OTNStreamReader::FindObject(const std::string& name) const {
		auto it = m_objectIndex.find(name);
		return it != m_objectIndex.end() ? &m_objects[it->second] : nullptr;
	}

	void OTNStreamReader::SetCurrentObject(size_t objectIndex, size_t row) {
		const ObjectInfo& info = m_objects[objectIndex];
		m_currentObject = objectIndex;
		m_currentRow = row;
		m_currentName = info.header.name;
		m_columnNames = info.header.columnNames;
		m_columnTypes = info.header.columnTypes;
		m_remainingRows = info.header.rowCount - row;
	}

	// Positions the tokenizer at the start of the given row (row == rowCount: behind the last row).
	bool OTNStreamReader::SeekToRow(const ObjectInfo& info, size_t row) {
		size_t checkpoint = row / ROW_CHECKPOINT_INTERVAL;
		if (checkpoint >= info.rowCheckpoints.size())
			checkpoint = info.rowCheckpoints.size() - 1;

		m_tok->Seek(info.rowCheckpoints[checkpoint]);
		for (size_t r = checkpoint * ROW_CHECKPOINT_INTERVAL; r < row; ++r) {
			if (!m_syntax->skipRow(*m_tok))
				return TokenizerFailed();
		}

		if (!m_tok->IsValid())
			return TokenizerFailed();
		return m_valid;
	}

	bool OTNStreamReader::ReadRowValues(const ObjectInfo& info, OTNRow& outRow) {
		std::optional<OTNRow> row = m_syntax->readRow(*m_tok, info.header.columnTypes);
		if (!row)
			return TokenizerFailed();

		outRow = std::move(*row);
		return m_valid;
	}

	bool OTNStreamReader::NextObject() {
		if (!EnsureUpToDate())
			return false;

		size_t next = (m_currentObject == NO_OBJECT) ? 0 : m_currentObject + 1;
		if (next >= m_objects.size()) {
			// stay behind the last object
			if (m_currentObject != NO_OBJECT)
				m_remainingRows = 0;
			return false;
		}

		// remaining rows of the previous object are skipped by seeking to the next object
		SetCurrentObject(next, 0);
		return SeekToRow(m_objects[next], 0);
	}

	bool OTNStreamReader::ReadRow(OTNRow& outRow) {
		if (!EnsureUpToDate())
			return false;
		if (m_currentObject == NO_OBJECT || m_remainingRows == 0)
			return false;

		if (!ReadRowValues(m_objects[m_currentObject], outRow))
			return false;

		m_currentRow++;
		m_remainingRows--;

		if (m_resolveObjectRefs) {
			std::vector<RowKey> stack;
			stack.push_back({ m_currentName, m_currentRow - 1 });

			// resolving seeks to other rows, continue here afterwards
			const Detail::FilePosition pos = m_tok->Tell();
			bool resolved = ResolveRow(outRow, stack);
			m_tok->Seek(pos);
			if (!resolved)
				return false;
		}

		return m_valid;
	}

	bool OTNStreamReader::SkipCurrentObject() {
		if (!EnsureUpToDate())
			return false;
		if (m_currentObject == NO_OBJECT)
			return true;

		const ObjectInfo& info = m_objects[m_currentObject];
		m_currentRow = info.header.rowCount;
		m_remainingRows = 0;
		return SeekToRow(info, info.header.rowCount);
	}

	std::vector<std::string> OTNStreamReader::GetObjectNames() {
		std::vector<std::string> names;
		if (!EnsureUpToDate())
			return names;

		names.reserve(m_objects.size());
		for (const auto& info : m_objects)
			names.push_back(info.header.name);
		return names;
	}

	std::optional<OTNObject> OTNStreamReader::ReadObjectRow(const std::string& objectName, size_t rowIndex) {
		if (!EnsureUpToDate())
			return std::nullopt;

		const ObjectInfo* info = FindObject(objectName);
		if (!info || rowIndex >= info->header.rowCount)
			return std::nullopt;

		std::vector<RowKey> stack;
		return LoadObjectRow(*info, rowIndex, stack);
	}

	std::optional<OTNObject> OTNStreamReader::ReadObject(const std::string& objectName) {
		if (!EnsureUpToDate())
			return std::nullopt;

		const ObjectInfo* info = FindObject(objectName);
		if (!info)
			return std::nullopt;

		const Detail::FilePosition pos = m_tok->Tell();

		OTNObject obj(info->header.name);
		obj.SetNamesList(info->header.columnNames);
		obj.SetTypeDescList(info->header.columnTypes);
		obj.ReserveDataRows(info->header.rowCount);

		bool ok = SeekToRow(*info, 0);
		for (size_t r = 0; ok && r < info->header.rowCount; ++r) {
			OTNRow row;
			ok = ReadRowValues(*info, row);
			if (!ok)
				break;

			if (m_resolveObjectRefs) {
				std::vector<RowKey> stack;
				stack.push_back({ info->header.name, r });

				const Detail::FilePosition next = m_tok->Tell();
				ok = ResolveRow(row, stack);
				m_tok->Seek(next);
			}

			if (ok)
				obj.AddDataRowList(row);
		}

		m_tok->Seek(pos);
		if (!ok)
			return std::nullopt;

		if (!obj.IsValid()) {
			AddError("OTNStreamReader: object '" + info->header.name + "' is invalid: " + obj.GetError());
			return std::nullopt;
		}
		return obj;
	}

	// Loads one row as OTNObject, restores the read position afterwards.
	std::optional<OTNObject> OTNStreamReader::LoadObjectRow(const ObjectInfo& info, size_t row, std::vector<RowKey>& stack) {
		for (const RowKey& key : stack) {
			if (key.row == row && key.objectName == info.header.name) {
				AddError("OTNStreamReader: cyclic object reference to row " + std::to_string(row) +
					" of object '" + info.header.name + "'");
				return std::nullopt;
			}
		}

		const Detail::FilePosition pos = m_tok->Tell();

		OTNRow values;
		bool ok = SeekToRow(info, row) && ReadRowValues(info, values);

		if (ok && m_resolveObjectRefs) {
			stack.push_back({ info.header.name, row });
			ok = ResolveRow(values, stack);
			stack.pop_back();
		}

		m_tok->Seek(pos);
		if (!ok)
			return std::nullopt;

		OTNObject obj(info.header.name);
		obj.SetNamesList(info.header.columnNames);
		obj.SetTypeDescList(info.header.columnTypes);
		obj.AddDataRowList(values);

		if (!obj.IsValid()) {
			AddError("OTNStreamReader: row " + std::to_string(row) + " of object '" +
				info.header.name + "' is invalid: " + obj.GetError());
			return std::nullopt;
		}
		return obj;
	}

	bool OTNStreamReader::ResolveRow(OTNRow& row, std::vector<RowKey>& stack) {
		for (OTNValue& value : row) {
			if (!ResolveValue(value, stack))
				return false;
		}
		return true;
	}

	bool OTNStreamReader::ResolveValue(OTNValue& value, std::vector<RowKey>& stack) {
		if (value.type == OTNBaseType::LIST) {
			const auto& list = std::get<Detail::OTNArrayPtr>(value.value);
			if (!list)
				return true;

			for (OTNValue& elem : list->values) {
				if (!ResolveValue(elem, stack))
					return false;
			}
			return true;
		}

		if (value.type != OTNBaseType::OBJECT_REF)
			return true;

		const Detail::OTNObjectRef& ref = std::get<Detail::OTNObjectRef>(value.value);
		const ObjectInfo* target = FindObject(ref.refObjectName);
		if (!target) {
			AddError("OTNStreamReader: object reference could not be resolved: no object named '" +
				ref.refObjectName + "' exists (referenced from object '" + stack.front().objectName + "')");
			return false;
		}

		if (ref.index >= target->header.rowCount) {
			AddError("OTNStreamReader: object reference index out of bounds: index " +
				std::to_string(ref.index) + " for object '" + ref.refObjectName +
				"' (referenced from object '" + stack.front().objectName + "')");
			return false;
		}

		std::optional<OTNObject> resolved = LoadObjectRow(*target, ref.index, stack);
		if (!resolved)
			return false;

		value = OTNValue(std::make_shared<OTNObject>(std::move(*resolved)));
		return true;
	}

	bool OTNStreamReader::HasMoreObjects() const {
		if (!m_valid)
			return false;

		bool nextObject = (m_currentObject == NO_OBJECT)
			? !m_objects.empty()
			: m_currentObject + 1 < m_objects.size();
		return nextObject || m_remainingRows > 0;
	}

	bool OTNStreamReader::HasMoreRows() const {
		return m_valid && m_remainingRows > 0;
	}

	const std::string& OTNStreamReader::GetCurrentObjectName() const {
		return m_currentName;
	}

	const std::vector<std::string>& OTNStreamReader::GetColumnNames() const {
		return m_columnNames;
	}

	const std::vector<OTNTypeDesc>& OTNStreamReader::GetColumnTypes() const {
		return m_columnTypes;
	}

	size_t OTNStreamReader::GetRemainingRowCount() const {
		return m_remainingRows;
	}

	void OTNStreamReader::SetResolveObjectRefs(bool value) {
		m_resolveObjectRefs = value;
	}

	bool OTNStreamReader::GetResolveObjectRefs() const {
		return m_resolveObjectRefs;
	}

	uint8_t OTNStreamReader::GetVersion() const {
		return m_fileHeader.version;
	}

	bool OTNStreamReader::IsValid() const {
		return m_valid;
	}

	const std::string& OTNStreamReader::GetError() const {
		return m_error;
	}

	bool OTNStreamReader::TryGetError(std::string& outError) const {
		if (m_valid)
			return false;

		outError = m_error;
		return true;
	}

	bool OTNStreamReader::TokenizerFailed() {
		if (m_tok && !m_tok->IsValid())
			AddError(m_tok->GetError());
		else
			AddError("OTNStreamReader: failed to read the file");
		return false;
	}

	void OTNStreamReader::AddError(const std::string& msg) {
		if (!m_error.empty())
			m_error.push_back('\n');
		m_error += msg;
		m_valid = false;
	}

#pragma endregion

}