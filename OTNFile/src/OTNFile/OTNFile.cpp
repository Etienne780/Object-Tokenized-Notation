#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <cassert>

#include "OTNFile.h"

namespace OTN {

	static inline void HashCombine(size_t& seed, size_t value) {
		seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
	}

	// returns OTNBasType or UNKNOWN
	OTNBaseType StringToOTNBaseType(const std::string& type) {
		namespace SynTypes = Syntax::Types;

		static const std::unordered_map<std::string_view, OTNBaseType> table = {
			{ SynTypes::INT,  OTNBaseType::INT },
			{ SynTypes::INT64,  OTNBaseType::INT64 },
			{ SynTypes::UINT64,  OTNBaseType::UINT64 },
			{ SynTypes::FLOAT, OTNBaseType::FLOAT },
			{ SynTypes::DOUBLE, OTNBaseType::DOUBLE },
			{ SynTypes::BOOL,   OTNBaseType::BOOL },
			{ SynTypes::STRING,   OTNBaseType::STRING },
			{ SynTypes::OBJECT,   OTNBaseType::OBJECT },
			{ SynTypes::OBJECT_REF, OTNBaseType::OBJECT_REF },
			{ SynTypes::LIST,   OTNBaseType::LIST },
			{ SynTypes::ANY,   OTNBaseType::ANY }
		};

		auto it = table.find(type);
		return it != table.end() ? it->second : OTNBaseType::UNKNOWN;
	}

	constexpr std::string_view OTNValueTypeToString(OTNBaseType type) noexcept {
		namespace SynTypes = Syntax::Types;

		switch (type) {
		case OTN::OTNBaseType::INT:     return SynTypes::INT;
		case OTN::OTNBaseType::INT64:     return SynTypes::INT64;
		case OTN::OTNBaseType::UINT64:     return SynTypes::UINT64;
		case OTN::OTNBaseType::FLOAT:   return SynTypes::FLOAT;
		case OTN::OTNBaseType::DOUBLE:  return SynTypes::DOUBLE;
		case OTN::OTNBaseType::BOOL:    return SynTypes::BOOL;
		case OTN::OTNBaseType::STRING:  return SynTypes::STRING;
		case OTN::OTNBaseType::OBJECT:  return SynTypes::OBJECT;
		case OTN::OTNBaseType::OBJECT_REF: return SynTypes::OBJECT_REF;
		case OTN::OTNBaseType::LIST:    return SynTypes::LIST;
		case OTN::OTNBaseType::ANY:    return SynTypes::ANY;
		case OTN::OTNBaseType::UNKNOWN:
		default:                         return "UNKNOWN";
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

	static inline void AppendRefName(
		std::string& out,
		const std::string& objectName
	) {
		out.append(Keyword::REF_KW);
		out.push_back(Syntax::REF_BEGIN_CHAR);
		out.append(objectName);
		out.push_back(Syntax::REF_END_CHAR);
	}

	// Shared value -> string serializer (used by OTNWriter and OTNStreamWriter)
	template<typename T>
	inline void OTNAppendNumber(std::string& out, T value) {
		char buf[64]{};
		auto result = std::to_chars(buf, buf + sizeof(buf), value);
		if (result.ec == std::errc())
			out.append(buf, result.ptr);
	}

	template<>
	inline void OTNAppendNumber<float>(std::string& out, float value) {
		char buf[64];
		auto result = std::to_chars(
			buf,
			buf + sizeof(buf),
			value,
			std::chars_format::general,
			std::numeric_limits<float>::max_digits10
		);

		if (result.ec == std::errc())
			out.append(buf, result.ptr);
	}

	template<>
	inline void OTNAppendNumber<double>(std::string& out, double value) {
		char buf[64];
		auto result = std::to_chars(
			buf,
			buf + sizeof(buf),
			value,
			std::chars_format::general,
			std::numeric_limits<double>::max_digits10
		);

		if (result.ec == std::errc())
			out.append(buf, result.ptr);
	}

	static void OTNAppendValueToString(std::string& out, const OTNValue& data) {
		switch (data.type) {
		case OTNBaseType::INT:
			OTNAppendNumber(out, std::get<int>(data.value));
			break;
		case OTNBaseType::INT64:
			OTNAppendNumber(out, std::get<int64_t>(data.value));
			break;
		case OTNBaseType::UINT64:
			OTNAppendNumber(out, std::get<uint64_t>(data.value));
			break;
		case OTNBaseType::FLOAT:
			OTNAppendNumber(out, std::get<float>(data.value));
			break;
		case OTNBaseType::DOUBLE:
			OTNAppendNumber(out, std::get<double>(data.value));
			break;
		case OTNBaseType::BOOL:
			out.append(
				std::get<bool>(data.value)
				? Keyword::TRUE_KW
				: Keyword::FALSE_KW
			);
			break;
		case OTNBaseType::STRING:
			out.push_back('"');
			out += std::get<std::string>(data.value);
			out.push_back('"');
			break;
		case OTNBaseType::LIST: {
			const auto& arr = std::get<OTNArrayPtr>(data.value);
			out.push_back(Syntax::LIST_BEGIN_CHAR);
			if (arr) {
				bool first = true;
				for (const auto& v : arr->values) {
					if (!first) {
						out.push_back(Syntax::SEPARATOR_CHAR);
						out.push_back(' ');
					}
					first = false;
					OTNAppendValueToString(out, v);
				}
			}
			out.push_back(Syntax::LIST_END_CHAR);
			break;
		}
		default:
			break;
		}
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
			const auto& arr = std::get<OTNArrayPtr>(current->value);
			if (!arr || arr->values.empty()) {
				desc.baseType = OTNBaseType::UNKNOWN;
				break;
			}
			current = &arr->values.front();
			desc.baseType = current->type;
		}

		if (desc.baseType == OTNBaseType::OBJECT) {
			const auto& obj = std::get<OTNObjectPtr>(current->value);
			if (obj)
				desc.refObjectName = obj->GetObjectName();
		}

		if (desc.baseType == OTNBaseType::OBJECT_REF) {
			const auto& obj = std::get<OTNObjectRef>(current->value);
			desc.baseType = OTNBaseType::OBJECT;
			desc.refObjectName = obj.refObjectName;
		}

		return desc;
	}

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
		m_wasBuildFromT(other.m_wasBuildFromT) {
	}

	OTNObject& OTNObject::operator=(const OTNObject& other) noexcept {
		m_name = other.m_name;
		m_columnNames = other.m_columnNames;
		m_columnTypes = other.m_columnTypes;
		m_dataRows = other.m_dataRows;
		m_error = other.m_error;
		m_valid = other.m_valid;
		m_wasBuildFromT = other.m_wasBuildFromT;
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

		if (rowValid) {
			m_dataRows.emplace_back(std::move(row));
		}

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

		uint32_t listDepth = 0;
		size_t pos = t.size();

		// Count trailing [] pairs
		while (pos >= 2 && t[pos - 2] == '[' && t[pos - 1] == ']') {
			listDepth++;
			pos -= 2;
		}

		// Remaining part is the base type
		const std::string baseTypeStr = t.substr(0, pos);

		if (baseTypeStr.empty()) {
			AddError("Missing base type in '" + t + "'!");
			return false;
		}

		if (!DebugIsNameValid(baseTypeStr)) {
			AddError("Type " + baseTypeStr + " contains invalid chars!");
			return false;
		}

		OTNBaseType baseType = StringToOTNBaseType(baseTypeStr);
		tempList.emplace_back(baseType, listDepth);

		// if types was unkown, type could be an object name
		if (baseType == OTNBaseType::UNKNOWN) {
			auto& ref = tempList.back();
			ref.baseType = OTNBaseType::OBJECT;
			ref.refObjectName = baseTypeStr;
		}
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

			OTNArrayPtr arrayPtr = std::get<OTNArrayPtr>(value.value);
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

		auto& stream = m_writerData.stream.stream;
		stream.open(path, std::ios::binary);

		if (!stream.is_open()) {
			return false;
		}

		if (!CreateWriteData(m_writerData)) {
			stream.close();
			return false;
		}

		if (!WriteHeader()) {
			stream.close();
			return false;
		}

		if (!WriteBody()) {
			stream.close();
			return false;
		}

		m_writerData.stream.Flush();
		stream.close();
		return true;
	}

	bool OTNWriter::WriteToString(std::string& outText) {
		m_writerData.Reset();
		outText.clear();

		if (!CreateWriteData(m_writerData)) {
			return false;
		}

		if (!WriteHeader()) {
			return false;
		}

		if (!WriteBody()) {
			return false;
		}

		outText = m_writerData.stream.buffer;
		m_writerData.stream.buffer.clear();
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

			const auto& arr = std::get<OTNArrayPtr>(current->value);
			if (!arr || arr->values.empty())
				break;

			current = &arr->values.front();
		}

		result.baseType = current->type;

		if (current->type == OTNBaseType::OBJECT) {
			const auto& obj = std::get<OTNObjectPtr>(current->value);
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
		m_objects.clear();

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
			const OTNArrayPtr& arrayPtr = std::get<OTNArrayPtr>(val.value);
			OTNArrayPtr newArray = std::make_shared<OTNArray>();
			if (!arrayPtr || !newArray)
				return;

			newArray->values.reserve(arrayPtr->values.size());

			for (const auto& v : arrayPtr->values) {
				if (v.type == OTNBaseType::OBJECT) {
					OTNValue converted;
					ConvertToSerValue(data, converted, colType, v);
					if (converted.type == OTNBaseType::LIST) {
						const OTNArrayPtr& indices = std::get<OTNArrayPtr>(converted.value);
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
			const OTNObjectPtr& objPtr = std::get<OTNObjectPtr>(val.value);
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
				OTNArrayPtr newArray = std::make_shared<OTNArray>();
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
		auto& defTypeMap = m_writerData.defType;
		defTypeMap.clear();
		uint32_t indexCount = 0;

		for (const auto& [type, used] : m_writerData.typeUsage) {
			if (type == OTNBaseType::OBJECT || type == OTNBaseType::LIST)
				continue;

			uint32_t length = OTNValueTypeCharLength(type);

			if (used > 1 && length * used > 9)
				defTypeMap[std::string(OTNValueTypeToString(type))] = indexCount++;
		}

		return true;
	}

	bool OTNWriter::CreateDefName() {
		auto& defNameMap = m_writerData.defName;
		defNameMap.clear();

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
				defNameMap.emplace(name, indexCount++);
			}
		}

		return true;
	}

	bool OTNWriter::WriteHeader() {
		auto& stream = m_writerData.stream;

		WriteDirective(stream, Keyword::VERSION_KW, [&]() {
			stream << static_cast<unsigned>(OTN::VERSION);
			});
		AddLineBreak(stream);

		if (m_useDefType) {
			if (!WriteHeaderDefType())
				return false;
		}

		if (m_useDefName) {
			if (!WriteHeaderDefName())
				return false;
		}

		AddLineBreak(stream);
		return true;
	}

	bool OTNWriter::WriteHeaderDefType() {
		auto& defTypeMap = m_writerData.defType;
		if (defTypeMap.empty())
			return true;

		auto& stream = m_writerData.stream;
		WriteDirective(stream, Keyword::DEF_TYPE_KW, [&]() {
			WriteHeaderDefHelper(stream, defTypeMap);
			});
		AddLineBreak(stream);

		return true;
	}

	bool OTNWriter::WriteHeaderDefName() {
		auto& defNameMap = m_writerData.defName;
		if (defNameMap.empty())
			return true;

		auto& stream = m_writerData.stream;

		WriteDirective(stream, Keyword::DEF_NAME_KW, [&]() {
			WriteHeaderDefHelper(stream, defNameMap);
			});
		AddLineBreak(stream);

		return true;
	}

	void OTNWriter::WriteHeaderDefHelper(BufferedIndentedStream& stream, const std::unordered_map<std::string, uint32_t>& map) {
		bool first = true;
		for (const auto& [name, id] : map) {
			if (!first) {
				stream << Syntax::SEPARATOR_CHAR;
				AddSpace(stream);
			}
			first = false;

			stream << name;
			AddSpace(stream);
			stream << Syntax::ASSIGNMENT_CHAR;
			AddSpace(stream);
			stream << id;
		}
	}

	bool OTNWriter::WriteBody() {
		auto& stream = m_writerData.stream;

		bool valid = true;
		WriteDirective(stream, Keyword::OBJECT_KW, [&]() {
			stream << Syntax::BLOCK_BEGIN_CHAR;
			AddLineBreak(stream);
			if (!m_useOptimizations)
				stream.IncreaseIndent();

			if (!WriteObjects(stream, m_writerData.objects))
				valid = false;
			if (!m_useOptimizations)
				stream.DecreaseIndent();

			stream << Syntax::BLOCK_END_CHAR;
			});

		return valid;
	}

	bool OTNWriter::WriteObjects(BufferedIndentedStream& stream,
		const std::unordered_map<std::string, SerializedObject>& objects) {
		const auto& defNameMap = m_writerData.defName;
		const auto& defTypeMap = m_writerData.defType;

		auto writeNames = [&](const SerializedObject& obj) {
			std::string out;
			out.reserve(obj.columnNames.size() * 16);

			bool firstName = true;
			for (size_t i = 0; i < obj.columnNames.size(); ++i) {
				if (!firstName) {
					out += Syntax::SEPARATOR_CHAR;
					AddSpace(out);
				}
				firstName = false;

				const OTNTypeDesc& colType = obj.columnTypes[i];

				if (colType.refObjectName.empty()) {
					const std::string_view baseTypeStr = OTNValueTypeToString(colType.baseType);

					if (defTypeMap.empty()) {
						out += baseTypeStr;
					}
					else {
						std::string str(baseTypeStr);
						auto it = defTypeMap.find(str);
						if (it != defTypeMap.end()) {
							out += std::to_string(it->second);
						}
						else {
							out += baseTypeStr;
						}
					}
				}
				else {
					AppendRefName(out, colType.refObjectName);
				}

				for (size_t j = 0; j < colType.listDepth; ++j) {
					out += "[]";
				}

				out += Syntax::TYPE_SEPARATOR_CHAR;

				// ---------- NAME ----------
				const std::string& colName = obj.columnNames[i];

				if (defNameMap.empty()) {
					out += colName;
				}
				else {
					auto it = defNameMap.find(colName);
					if (it != defNameMap.end()) {
						out += std::to_string(it->second);
					}
					else {
						out += colName;
					}
				}
			}

			stream << out;
			};

		bool firstObj = true;
		for (const auto& [name, obj] : objects) {
			if (!firstObj) {
				AddLineBreak(stream);
			}
			firstObj = false;
			stream << name;
			stream << "[";
			stream << obj.rows.size();
			stream << "]";
			AddSpace(stream);
			stream << Syntax::BLOCK_BEGIN_CHAR;
			AddLineBreak(stream);

			AddIndent(stream);
			if (obj.columnNames.size() != obj.columnTypes.size()) {
				AddError("Could not save body Section @Object, size of names(" +
					std::to_string(obj.columnNames.size())
					+ ") and types(" +
					std::to_string(obj.columnTypes.size())
					+ ") dose not match in object '" + name + "'!");
				return false;
			}

			writeNames(obj);
			AddLineBreak(stream);
			stream
				<< Syntax::BLOCK_END_CHAR
				<< Syntax::STATEMENT_TERMINATOR;
			AddLineBreak(stream);

			size_t rowLength = (obj.rows.size() > 0) ? obj.rows[0].size() : 0;
			std::string rowOutStr;
			rowOutStr.reserve(rowLength * 8);

			for (const auto& row : obj.rows) {
				bool first = true;
				for (const auto& serValue : row) {
					if (!first) {
						rowOutStr += Syntax::SEPARATOR_CHAR;
						AddSpace(rowOutStr);
					}
					first = false;

					WriteOTNValueData(rowOutStr, serValue);
				}
				rowOutStr += Syntax::STATEMENT_TERMINATOR;
				stream << rowOutStr;
				AddLineBreak(stream);
				rowOutStr.clear();
			}
		}

		return true;
	}

	void OTNWriter::WriteOTNValueData(std::string& outStr, const OTNValue& data) {
		switch (data.type) {
		case OTNBaseType::INT:
			WriteData(outStr, std::get<int>(data.value));
			break;
		case OTNBaseType::INT64:
			WriteData(outStr, std::get<int64_t>(data.value));
			break;
		case OTNBaseType::UINT64:
			WriteData(outStr, std::get<uint64_t>(data.value));
			break;
		case OTNBaseType::FLOAT:
			WriteData(outStr, std::get<float>(data.value));
			break;
		case OTNBaseType::DOUBLE:
			WriteData(outStr, std::get<double>(data.value));
			break;
		case OTNBaseType::BOOL:
			WriteData(outStr, std::get<bool>(data.value));
			break;
		case OTNBaseType::STRING:
			outStr += '"' + std::get<std::string>(data.value) + '"';
			break;
		case OTNBaseType::LIST: {
			auto& array = std::get<OTNArrayPtr>(data.value);
			outStr += Syntax::LIST_BEGIN_CHAR;
			if (array) {
				bool first = true;
				for (const auto& val : array->values) {
					if (!first) {
						outStr += Syntax::SEPARATOR_CHAR;
						AddSpace(outStr);
					}
					first = false;
					WriteOTNValueData(outStr, val);
				}
			}
			outStr += Syntax::LIST_END_CHAR;
			break;
		}
		case OTNBaseType::OBJECT:
		case OTNBaseType::UNKNOWN:
		default:
			AddError("WriteData: unsupported OTNValueType");
#ifndef NDEBUG
			assert(false && "WriteData: unsupported OTNValueType");
#endif
			break;
		}
	}

	template<typename T>
	void OTNWriter::WriteData(std::string& outStr, const T& data) {
		using DT = std::decay_t<T>;

		if constexpr (std::is_same_v<DT, int>) {
			outStr += ToString(data);
		}
		else if constexpr (std::is_same_v<DT, int64_t>) {
			outStr += ToString(data);
		}
		else if constexpr (std::is_same_v<DT, uint64_t>) {
			outStr += ToString(data);
		}
		else if constexpr (std::is_same_v<DT, float>) {
			outStr += ToString(data);
		}
		else if constexpr (std::is_same_v<DT, double>) {
			outStr += ToString(data);
		}
		else if constexpr (std::is_same_v<DT, bool>) {
			outStr += (data ? Keyword::TRUE_KW : Keyword::FALSE_KW);
		}
		else {
			static_assert(otn_always_false_v<DT>, "Unsupported type for WriteData");
		}
	}

	void OTNWriter::AddSpace(BufferedIndentedStream& stream) const {
		if (!m_useOptimizations)
			stream << ' ';
	}

	void OTNWriter::AddIndent(BufferedIndentedStream& stream, uint32_t level) const {
		if (m_useOptimizations)
			return;

		for (; level > 0; --level)
			stream << '\t';
	}

	void OTNWriter::AddLineBreak(BufferedIndentedStream& stream) const {
		if (!m_useOptimizations)
			stream << '\n';
		stream.NewLine();
	}

	void OTNWriter::AddSpace(std::string& outStr) const {
		if (!m_useOptimizations)
			outStr.push_back(' ');
	}

	void OTNWriter::AddIndent(std::string& outStr, uint32_t level) const {
		if (m_useOptimizations)
			return;

		for (; level > 0; --level)
			outStr.push_back('\t');
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

	bool OTNReader::OTNTokenizer::Tokenize() {
		char c;
		while (Get(c)) {
			if (!ProcessChar(c)) {
				return false;
			}
		}

		AddToken(TokenType::END_OF_FILE);
		return true;
	}

	const std::vector<OTNReader::Token>& OTNReader::OTNTokenizer::GetTokens() const {
		return m_tokens;
	}

	std::string OTNReader::OTNTokenizer::GetError() const {
		return m_error;
	}

	bool OTNReader::OTNTokenizer::Get(char& c) {
		if (m_cur >= m_end) 
			return false;

		c = *m_cur++;
		return true;
	}

	void OTNReader::OTNTokenizer::Unget() {
		m_cur--; 
	}

	int OTNReader::OTNTokenizer::Peek() const { 
		return m_cur < m_end ? static_cast<unsigned char>(*m_cur) : -1; 
	}

	bool OTNReader::OTNTokenizer::Eof() const { 
		return m_cur >= m_end; 
	}

	bool OTNReader::OTNTokenizer::ProcessChar(char c) {
		Advance(c);

		// skip whitespace
		if (std::isspace(static_cast<unsigned char>(c)))
			return true;

		switch (c) {
		case Syntax::STATEMENT_TERMINATOR:
			AddToken(TokenType::SEMICOLON);
			break;

		case Syntax::KEYWORD_PREFIX_CHAR:
			AddToken(TokenType::KEYWORD_PREFIX);
			break;

		case Syntax::KEYWORD_ASSIGN_CHAR:
			AddToken(TokenType::COLON);
			break;

		case Syntax::ASSIGNMENT_CHAR:
			AddToken(TokenType::EQUALS);
			break;

		case Syntax::SEPARATOR_CHAR:
			AddToken(TokenType::COMMA);
			break;

		case Syntax::TYPE_SEPARATOR_CHAR:
			AddToken(TokenType::SLASH);
			break;

		case Syntax::BLOCK_BEGIN_CHAR:
			AddToken(TokenType::BLOCK_BEGIN);
			break;

		case Syntax::BLOCK_END_CHAR:
			AddToken(TokenType::BLOCK_END);
			break;

		case Syntax::LIST_BEGIN_CHAR:
			AddToken(TokenType::LIST_BEGIN);
			break;

		case Syntax::LIST_END_CHAR:
			AddToken(TokenType::LIST_END);
			break;

		case Syntax::REF_BEGIN_CHAR:
			AddToken(TokenType::REF_BEGIN);
			break;

		case Syntax::REF_END_CHAR:
			AddToken(TokenType::REF_END);
			break;

		case '"':
			if (!ReadString())
				return false;
			break;

		default:
			if (c == '-') {
				AddToken(TokenType::MINUS, m_line, m_column);
			}
			else if (std::isdigit(static_cast<unsigned char>(c))) {
				Unget();
				m_column--;
				if (!ReadNumber())
					return false;
			}
			else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
				Unget();
				m_column--;
				if (!ReadIdentifier())
					return false;
			}
			else {
				AddError("Unexpected characterat line "
					+ std::to_string(m_line) + ", column "
					+ std::to_string(m_column) + ": '" + std::string(1, c) + "' line");
				return false;
			}
			break;
		}
		return true;
	}

	bool OTNReader::OTNTokenizer::AddToken(TokenType type) {
		m_tokens.emplace_back(type, m_line, m_column);
		return true;
	}

	bool OTNReader::OTNTokenizer::AddToken(TokenType type, const std::string& text) {
		return AddToken(type, text, m_line, m_column);
	}

	bool OTNReader::OTNTokenizer::AddToken(
		TokenType type,
		uint32_t line,
		uint32_t column)
	{
		m_tokens.emplace_back(type, line, column);
		return true;
	}

	bool OTNReader::OTNTokenizer::AddToken(
		TokenType type,
		const std::string& text,
		uint32_t line,
		uint32_t column)
	{
		m_tokens.emplace_back(type, text, line, column);
		return true;
	}

	bool OTNReader::OTNTokenizer::ReadString() {
		char c;
		std::string stringText;
		stringText.reserve(32);

		uint32_t startLine = m_line;
		uint32_t startColumn = m_column;// position of opening quote

		bool closed = false;

		while (Get(c)) {
			Advance(c);

			if (c == '"') {
				closed = true;
				break;
			}

			if (c == '\\') { // escape handling
				if (!Get(c))
					break;
				Advance(c);

				switch (c) {
				case 'n':  stringText.push_back('\n'); break;
				case 't':  stringText.push_back('\t'); break;
				case '"':  stringText.push_back('"');  break;
				case '\\': stringText.push_back('\\'); break;
				default:
					AddError("Invalid escape sequence");
					return false;
				}
			}
			else {
				stringText.push_back(c);
			}
		}

		if (!closed) {
			AddError("Unterminated string literal");
			return false;
		}

		AddToken(TokenType::STRING, stringText, startLine, startColumn);
		return true;
	}

	bool OTNReader::OTNTokenizer::ReadNumber() {
		char c;
		std::string textNumber;
		textNumber.reserve(20);

		uint32_t startLine = m_line;
		uint32_t startColumn = m_column;

		bool hasDot = false;
		bool hasExp = false;

		while (Get(c)) {
			if (c == '.') {
				if (hasDot || hasExp) { // dot in exponent invalid
					AddError("Invalid number format");
					return false;
				}
				hasDot = true;
				textNumber.push_back(c);
				Advance(c);
				continue;
			}

			if (c == 'e' || c == 'E') {
				if (hasExp) {
					AddError("Invalid number format: multiple exponents");
					return false;
				}
				hasExp = true;
				textNumber.push_back(c);
				Advance(c);

				// exponent can be + or -
				int nxt = Peek();
				if (nxt == '+' || nxt == '-') {
					Get(c);
					textNumber.push_back(c);
					Advance(c);
				}
				continue;
			}

			if (!std::isdigit(static_cast<unsigned char>(c))) {
				Unget();
				break;
			}

			textNumber.push_back(c);
			Advance(c);
		}

		AddToken(TokenType::NUMBER, textNumber, startLine, startColumn);
		return true;
	}

	bool OTNReader::OTNTokenizer::ReadIdentifier() {
		char c;
		std::string text;
		text.reserve(32);

		uint32_t startLine = m_line;
		uint32_t startColumn = m_column;

		while (Get(c)) {
			if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
				Unget();
				break;
			}

			text.push_back(c);
			Advance(c);
		}

		AddToken(TokenType::IDENTIFIER, text, startLine, startColumn);
		return true;
	}

	void OTNReader::OTNTokenizer::Advance(char c) {
		if (c == '\n') {
			m_line++;
			m_column = 1;
		}
		else {
			m_column++;
		}
	}

	void OTNReader::OTNTokenizer::AddError(const std::string& msg) {
		if (!m_error.empty())
			m_error.push_back('\n');
		m_error += msg;
		m_valid = false;
	}

#pragma region ReaderV_Num

	bool OTNReader::OTNReaderV1::Read() {
		while (!IsAtEnd()) {
			if (!IsValid())
				return false;

			if (!ParseTopLevel())
				return false;
		}

		if (!ResolveOTNObjectRefs())
			return false;

		return true;
	}

	std::string OTNReader::OTNReaderV1::GetError() const {
		return m_error;
	}

	bool OTNReader::OTNReaderV1::IsValid() const {
		return m_valid;
	}

	bool  OTNReader::OTNReaderV1::ParseTopLevel() {
		if (!Match(TokenType::KEYWORD_PREFIX)) {
			AddError(Peek(), "expected '" + std::string(1, Syntax::KEYWORD_PREFIX_CHAR) + "'");
			return false;
		}

		const Token& keyword = Next();
		TokenKeyword kw = ResolveKeyword(keyword);
		if (kw == TokenKeyword::UNKNOWN) {
			AddError(keyword, "unknown keyword");
			return false;
		}

		switch (kw) {
		case TokenKeyword::VERSION: {
			// skip version
			while (true) {
				TokenType t = Next().type;
				if (t == TokenType::SEMICOLON || t == TokenType::END_OF_FILE)
					break;
			}
			return true;
		}
		case TokenKeyword::DEF_TYPE:
			return ParseDefType();
		case TokenKeyword::DEF_NAME:
			return ParseDefName();
		case TokenKeyword::OBJECT:
			return ParseObjectBlock();
		default:
			return false;
		}
	}

	bool OTNReader::OTNReaderV1::ResolveOTNObjectRefs() {
		for (auto& [objectName, object] : m_data.objects) {
			if (!ResolveObjectRefsInObject(objectName, object))
				return false;
		}
		return true;
	}

	bool OTNReader::OTNReaderV1::ResolveObjectRefsInObject(
		const std::string& objectName,
		OTNObject& object)
	{
		auto& rows = object.GetDataRows();
		std::vector<size_t> refColumns = GetObjectIndieces(object);

		if (refColumns.empty())
			return true;

		for (auto& row : rows) {
			if (!ResolveObjectRefsInRow(objectName, refColumns, row))
				return false;
		}

		return true;
	}

	bool OTNReader::OTNReaderV1::ResolveObjectRefsInRow(
		const std::string& ownerObjectName,
		const std::vector<size_t>& refColumns,
		OTNRow& row)
	{
		for (auto& index : refColumns) {
			if (index >= row.size())
				break;

			if (!ResolveValueRecursive(ownerObjectName, row[index]))
				return false;
		}
		return true;
	}

	bool OTNReader::OTNReaderV1::ResolveValueRecursive(
		const std::string& ownerObjectName,
		OTNValue& value)
	{
		if (value.type == OTNBaseType::OBJECT_REF) {
			return ResolveSingleObjectRef(ownerObjectName, value);
		}

		if (value.type == OTNBaseType::OBJECT) {
			OTNObjectPtr& ptr = std::get<OTNObjectPtr>(value.value);
			if (!ptr) return true;
			return ResolveObjectRefsInObject(ptr->GetObjectName(), *ptr);
		}

		if (value.type == OTNBaseType::LIST) {
			OTNArrayPtr& arr = std::get<OTNArrayPtr>(value.value);
			if (!arr)
				return true;

			for (auto& elem : arr->values) {
				if (!ResolveValueRecursive(ownerObjectName, elem))
					return false;
			}
		}

		return true;
	}

	bool OTNReader::OTNReaderV1::ResolveSingleObjectRef(
		const std::string& ownerObjectName,
		OTNValue& value
	) {
		if (value.type != OTNBaseType::OBJECT_REF)
			return true;

		OTNObjectRef& ref = std::get<OTNObjectRef>(value.value);

		const OTNObject* targetObject = GetOTNObject(ref.refObjectName);
		if (!targetObject) {
			AddError(
				"Object reference could not be resolved: no object named '" +
				ref.refObjectName + "' exists (referenced from object '" +
				ownerObjectName + "')"
			);
			return false;
		}

		const auto& targetRows = targetObject->GetDataRows();
		if (targetRows.empty())
			return true;

		if (ref.index >= targetRows.size()) {
			AddError(
				"Object reference index out of bounds: index " +
				std::to_string(ref.index) +
				" for object '" + ref.refObjectName +
				"' (referenced from object '" + ownerObjectName + "')"
			);
			return false;
		}

		OTNObjectPtr resolved = std::make_shared<OTNObject>(ref.refObjectName);
		resolved->SetNamesList(targetObject->GetColumnNames());
		resolved->SetTypeDescList(targetObject->GetColumnTypesDesc());
		resolved->AddDataRowList(targetRows[ref.index]);

		// resolve object refs recurive
		ResolveObjectRefsInObject(ref.refObjectName, *resolved.get());

		value = OTNValue(resolved);
		return true;
	}

	const OTNObject* OTNReader::OTNReaderV1::GetOTNObject(const std::string& objName) const {
		auto it = m_data.objects.find(objName);
		return (it != m_data.objects.end()) ? &it->second : nullptr;
	}

	std::vector<size_t> OTNReader::OTNReaderV1::GetObjectIndieces(const OTNObject& obj) {
		std::vector<size_t> indices;
		const auto& types = obj.GetColumnTypesDesc();

		for (size_t i = 0; i < types.size(); i++) {
			if (types[i].baseType == OTNBaseType::OBJECT) {
				indices.push_back(i);
			}
		}

		return indices;
	}

	bool OTNReader::OTNReaderV1::ParseDefType() {
		Expect(TokenType::COLON);

		do {
			Token typeToken = Expect(TokenType::IDENTIFIER);
			Expect(TokenType::EQUALS);
			Token idToken = Expect(TokenType::NUMBER);

			if (!IsValid())
				return false;

			OTNBaseType type = StringToOTNBaseType(typeToken.text);
			if (type == OTNBaseType::UNKNOWN) {
				AddError(typeToken, "Failed to parse token to dataType!");
				return false;
			}

			int id = ParseNumericToken<int>(idToken, this);
			if (!IsValid())
				return false;

			m_data.defType[id] = typeToken.text;
		} while (NextIf(TokenType::COMMA));

		Expect(TokenType::SEMICOLON);
		return IsValid();
	}

	bool OTNReader::OTNReaderV1::ParseDefName() {
		Expect(TokenType::COLON);

		do {
			Token nameToken = Expect(TokenType::IDENTIFIER);
			Expect(TokenType::EQUALS);
			Token idToken = Expect(TokenType::NUMBER);

			if (!IsValid())
				return false;

			int id = ParseNumericToken<int>(idToken, this);
			if (!IsValid())
				return false;

			m_data.defName[id] = nameToken.text;
		} while (NextIf(TokenType::COMMA));

		Expect(TokenType::SEMICOLON);
		return IsValid();
	}

	bool OTNReader::OTNReaderV1::ParseObjectBlock() {
		Expect(TokenType::COLON);
		Expect(TokenType::BLOCK_BEGIN);

		if (!IsValid())
			return false;

		// parse objects
		while (Peek().type != TokenType::BLOCK_END) {
			if (!ParseObject())
				return false;
		}

		Expect(TokenType::BLOCK_END);
		Expect(TokenType::SEMICOLON);
		return IsValid();
	}

	bool OTNReader::OTNReaderV1::ParseObject() {
		const Token& objName = Expect(TokenType::IDENTIFIER);
		Expect(TokenType::LIST_BEGIN);
		const Token& objCount = Expect(TokenType::NUMBER);
		Expect(TokenType::LIST_END);

		if (!IsValid())
			return false;

		size_t count = 0;
		try {
			count = std::stoul(objCount.text);
		}
		catch (...) {
			AddError(objCount, "Invalid integer literal");
			return false;
		}

		OTNObject obj{ objName.text };
		if (!ParseHeaderBlock(obj))
			return false;

		if (!ParseDataRows(obj, count))
			return false;

		if (!obj.IsValid()) {
			AddError("Error while trying to read object '" + objName.text + "' error: " + obj.GetError());
			return false;
		}

		auto [it, inserted] =
			m_data.objects.emplace(objName.text, std::move(obj));

		if (!inserted) {
			AddError("Object '" + objName.text + "' already defined");
			return false;
		}
		return true;
	}

	bool OTNReader::OTNReaderV1::AddIdentifier(
		const Token& token,
		const std::unordered_map<uint32_t, std::string>& map,
		std::vector<std::string>& list)
	{
		int id = ParseNumericToken<int>(token, this);

		auto it = map.find(id);
		if (it == map.end()) {
			AddError(token, "Identifier not found in map");
			return false;
		}

		list.push_back(it->second);
		return true;
	}


	bool OTNReader::OTNReaderV1::ParseHeaderBlock(OTNObject& obj) {
		Expect(TokenType::BLOCK_BEGIN);

		std::vector<std::string> names;
		std::vector<std::string> types;
		names.reserve(4);
		types.reserve(4);

		while (Peek().type != TokenType::BLOCK_END) {
			Token typeToken, nameToken;
			uint32_t listDepth = 0;

			GetTypeToken(typeToken);
			bool isList = IsListToken(listDepth);

			Expect(TokenType::SLASH);
			nameToken = Next();

			if (!IsValid())
				return false;

			// Resolve type
			std::string typeName;
			if (typeToken.type == TokenType::IDENTIFIER) {
				typeName = typeToken.text;
			}
			else if (typeToken.type == TokenType::NUMBER) {
				if (!AddIdentifier(typeToken, m_data.defType, types))
					return false;
				typeName = types.back(); // already added
			}
			else {
				return AddError(typeToken, "invalid token type '" + typeToken.text + "'!");
			}

			// Apply list depth
			for (uint32_t d = 0; d < listDepth; ++d)
				typeName += "[]";
			if (typeToken.type == TokenType::IDENTIFIER)
				types.push_back(typeName);

			// Resolve name
			if (nameToken.type == TokenType::IDENTIFIER) {
				names.push_back(nameToken.text);
			}
			else if (nameToken.type == TokenType::NUMBER) {
				if (!AddIdentifier(nameToken, m_data.defName, names))
					return false;
			}
			else {
				return AddError(nameToken, "invalid token type '" + nameToken.text + "'!");
			}

			if (Peek().type == TokenType::COMMA) Next();
		}

		if (names.size() != types.size()) {
			return AddError("header column count mismatch");
		}

		Expect(TokenType::BLOCK_END);
		Expect(TokenType::SEMICOLON);

		obj.SetTypesList(types);
		obj.SetNamesList(names);
		return true;
	}

	bool OTNReader::OTNReaderV1::ParseDataRows(OTNObject& obj, size_t rowCount) {
		const std::vector<OTNTypeDesc>& types = obj.GetColumnTypesDesc();
		size_t pos = 0;
		size_t currentRowCount = 0;
		obj.ReserveDataRows(rowCount);

		while (currentRowCount < rowCount) {
			std::vector<OTNValue> values;
			values.reserve(types.size());
			do {
				if (pos >= types.size()) {
					AddError(Peek(), "Row '"
						+ std::to_string(currentRowCount)
						+ "' of object '" + obj.GetObjectName()
						+ "' has too many values! '"
						+ std::to_string(types.size())
						+ "' '" + std::to_string(pos) + "'");
					return false;
				}

				Token tok = Next();
				values.push_back(TokenToOTNValue(tok, types[pos]));
				if (!IsValid())
					return false;
				pos++;
			} while (NextIf(TokenType::COMMA));

			currentRowCount++;
			pos = 0;
			Expect(TokenType::SEMICOLON);
			obj.AddDataRowList(values);
		}
		return true;
	}

	const OTNReader::Token& OTNReader::OTNReaderV1::Peek() {
		if (m_pos >= m_tokens.size()) {
			static Token dummy;
			AddError("Peek: pos '"
				+ std::to_string(m_pos)
				+ "' was out of bounds for size '"
				+ std::to_string(m_tokens.size()));
			return dummy;
		}

		return m_tokens[m_pos];
	}

	const OTNReader::Token& OTNReader::OTNReaderV1::Next() {
		if (m_pos >= m_tokens.size()) {
			static Token dummy;
			AddError("Peek: pos '"
				+ std::to_string(m_pos)
				+ "' was out of bounds for size '"
				+ std::to_string(m_tokens.size()));
			return dummy;
		}

		return m_tokens[m_pos++];
	}

	bool OTNReader::OTNReaderV1::NextIf(TokenType type) {
		const Token& token = Next();
		if (token.type != type) {
			m_pos--;
			return false;
		}
		return true;
	}

	bool OTNReader::OTNReaderV1::Match(TokenType type) {
		if (Peek().type == type) {
			m_pos++;
			return true;
		}
		return false;
	}

	OTNReader::Token OTNReader::OTNReaderV1::Expect(TokenType type) {
		if (Peek().type != type) {
			AddError(Peek(), "unexpected token '" +
				(Peek().type == TokenType::IDENTIFIER ?
					ToString(Peek().type) + "(\"" + Peek().text + "\")" : ToString(Peek().type))
				+ "', expect token '"
				+ ToString(type) + "'");
		}
		return Next();
	}

	bool OTNReader::OTNReaderV1::IsAtEnd() {
		const Token& token = Peek();
		return token.type == TokenType::END_OF_FILE;
	}

	void OTNReader::OTNReaderV1::GetTypeToken(Token& outToken) {
		outToken = Next();

		// if token is not a reference to an object
		if (outToken.text != Keyword::REF_KW)
			return;

		Token temp = Next();
		if (temp.type != TokenType::REF_BEGIN) {
			AddError(temp, "Invalid Reference definition!");
			return;
		}

		temp = Next();
		if (temp.type != TokenType::IDENTIFIER) {
			AddError(temp, "Invalid Reference definition!");
			return;
		}

		outToken = std::move(temp);

		temp = Next();
		if (temp.type != TokenType::REF_END) {
			AddError(temp, "Invalid Refernce definition!");
			return;
		}
	}

	bool OTNReader::OTNReaderV1::IsListToken(uint32_t& outListDepth) {
		bool isList = false;

		while (Match(TokenType::LIST_BEGIN)) {
			if (!Match(TokenType::LIST_END)) {
				AddError("Invalid list tokens");
				break;
			}

			outListDepth++;
			isList = true;
		}

		return isList;
	}

	OTNValue OTNReader::OTNReaderV1::TokenToOTNValue(const Token& token, const OTNTypeDesc& type) {
		switch (type.baseType) {
		case OTN::OTNBaseType::INT:
		case OTN::OTNBaseType::INT64:
		case OTN::OTNBaseType::UINT64:
		case OTN::OTNBaseType::FLOAT:
		case OTN::OTNBaseType::DOUBLE:
		case OTN::OTNBaseType::BOOL:
		case OTN::OTNBaseType::STRING: {
			if (type.listDepth > 0) {
				return TokenToListOTNValue(token, type);
			}
			else {
				return TokenToPrimitiveOTNValue(token, type.baseType);
			}
		}
		case OTN::OTNBaseType::OBJECT:
			if (type.listDepth > 0) {
				return TokenToListOTNValue(token, type);
			}
			else {
				return TokenToObjectOTNValue(token, type);
			}
		case OTN::OTNBaseType::ANY:
			if (type.listDepth > 0)
				return TokenToListOTNValue(token, type);
			return TokenToAnyPrimitiveOTNValue(token);
		case OTN::OTNBaseType::LIST:
			AddError("Invalid type: TokenToOTNValue List!");
			break;
		case OTN::OTNBaseType::UNKNOWN:
		default:
			AddError("Invalid type: TokenToOTNValue UNKNOWN!");
			break;
		}
		return OTNValue{};
	}

	OTNValue OTNReader::OTNReaderV1::TokenToListOTNValue(
		const Token& token,
		const OTNTypeDesc& type)
	{
		if (token.type != TokenType::LIST_BEGIN) {
			AddError("Expected LIST_BEGIN token, but got type " + std::to_string((int)token.type));
			return OTNValue{};
		}

		OTNTypeDesc newType = type;
		newType.listDepth--;

		OTNArrayPtr list = std::make_shared<OTNArray>();
		size_t elementIndex = 0;

		do {
			Token tok = Next();

			if (tok.type == TokenType::END_OF_FILE) {
				AddError("Unexpected end of file while parsing list at element index " + std::to_string(elementIndex));
				return OTNValue{};
			}

			if (tok.type == TokenType::LIST_END)
				break;

			OTNValue value = TokenToOTNValue(tok, newType);
			if (!IsValid()) {
				AddError("Failed to parse element " + std::to_string(elementIndex) + " in list");
				return OTNValue{};
			}

			list->values.push_back(std::move(value));
			elementIndex++;


			NextIf(TokenType::COMMA);// skip comma
		} while (!Match(TokenType::LIST_END));

		return OTNValue{ list };
	}

	OTNValue OTNReader::OTNReaderV1::TokenToObjectOTNValue(
		const Token& token,
		const OTNTypeDesc& type)
	{
		// resolve Object, later
		if (token.type != TokenType::NUMBER) {
			AddError(token, "Expected integer number");
			return OTNValue{};
		}

		try {
			int value = std::stoi(token.text);
			return OTNValue(OTNObjectRef(type.refObjectName, value));
		}
		catch (...) {
			AddError(token, "Invalid integer literal");
			return {};
		}
	}

	OTNValue OTNReader::OTNReaderV1::TokenToAnyPrimitiveOTNValue(const Token& token) {
		switch (token.type) {
		case TokenType::STRING:
			return OTNValue(token.text);

		case TokenType::IDENTIFIER:
			if (token.text == Keyword::TRUE_KW)
				return OTNValue(true);

			if (token.text == Keyword::FALSE_KW)
				return OTNValue(false);

			AddError(token, "Unexpected identifier '" + token.text + "' in 'any' column");
			return OTNValue{};

		case TokenType::MINUS: {
			Token next = Next();
			if (next.type != TokenType::NUMBER) {
				AddError(next, "Expected number after '-' in 'any' column");
				return OTNValue{};
			}
			std::string text = "-" + next.text;
			bool isFloat = text.find('.') != std::string::npos ||
				text.find('e') != std::string::npos ||
				text.find('E') != std::string::npos;
			try {
				return isFloat ? OTNValue(std::stof(text)) : OTNValue(std::stoi(text));
			}
			catch (...) {
				AddError(next, "Invalid numeric literal in 'any' column: " + text);
				return OTNValue{};
			}
		}

		case TokenType::NUMBER: {
			const std::string& text = token.text;
			bool isFloat = text.find('.') != std::string::npos ||
				text.find('e') != std::string::npos ||
				text.find('E') != std::string::npos;
			try {
				return isFloat ? OTNValue(std::stof(text)) : OTNValue(std::stoi(text));
			}
			catch (...) {
				AddError(token, "Invalid numeric literal in 'any' column: " + text);
				return OTNValue{};
			}
		}

		case TokenType::LIST_BEGIN: {
			// Nested list inside an 'any' column – each element is also 'any'
			OTNTypeDesc anyListType{ OTNBaseType::ANY, 1 };
			return TokenToListOTNValue(token, anyListType);
		}

		default:
			AddError(token, "Unexpected token type for 'any' value");
			return OTNValue{};
		}
	}

	OTNValue OTNReader::OTNReaderV1::TokenToPrimitiveOTNValue(
		const Token& token,
		OTNBaseType expectedType)
	{
		switch (expectedType) {
		case OTNBaseType::INT:
			return OTNValue(ParseNumericToken<int>(token, this));
		case OTNBaseType::INT64:
			return OTNValue(ParseNumericToken<int64_t>(token, this));
		case OTNBaseType::UINT64:
			return OTNValue(ParseNumericToken<uint64_t>(token, this));
		case OTNBaseType::FLOAT:
			return OTNValue(ParseNumericToken<float>(token, this));
		case OTNBaseType::DOUBLE:
			return OTNValue(ParseNumericToken<double>(token, this));
		case OTNBaseType::BOOL: {
			if (!(token.type == TokenType::IDENTIFIER || token.type == TokenType::NUMBER)) {
				AddError(token, "Expected boolean literal");
				return OTNValue{};
			}

			if (token.type == TokenType::IDENTIFIER) {
				if (token.text == Keyword::TRUE_KW) {
					return OTNValue(true);
				}
				if (token.text == Keyword::FALSE_KW) {
					return OTNValue(false);
				}
			}
			else if (token.type == TokenType::NUMBER) {
				int boolNum = ParseNumericToken<int>(token, this);
				return OTNValue(static_cast<bool>(boolNum));
			}

			AddError(token, "Invalid boolean literal");
			return OTNValue{};
		}
		case OTNBaseType::STRING: {
			if (token.type != TokenType::STRING) {
				AddError(token, "Expected string literal");
				return OTNValue{};
			}

			return OTNValue(token.text);
		}
		case OTNBaseType::OBJECT: {
			AddError(token, "Object values must be parsed explicitly");
			return OTNValue{};
		}
		case OTNBaseType::LIST: {
			AddError(token, "List values must be parsed explicitly");
			return OTNValue{};
		}
		case OTNBaseType::UNKNOWN:
		default:
			AddError(token, "Unknown value type");
			return OTNValue{};
		}
	}

	OTNReader::TokenKeyword OTNReader::OTNReaderV1::OTNReaderV1::ResolveKeyword(const Token& token) {
		static const std::unordered_map<std::string_view, TokenKeyword> table = {
			{ Keyword::VERSION_KW,  TokenKeyword::VERSION },
			{ Keyword::DEF_TYPE_KW, TokenKeyword::DEF_TYPE },
			{ Keyword::DEF_NAME_KW, TokenKeyword::DEF_NAME },
			{ Keyword::OBJECT_KW,   TokenKeyword::OBJECT }
		};

		auto it = table.find(token.text);
		return it != table.end() ? it->second : TokenKeyword::UNKNOWN;
	}

	std::string OTNReader::OTNReaderV1::ToString(TokenType type) {
		switch (type) {
		case TokenType::UNKNOWN:        return "UNKNOWN";
		case TokenType::KEYWORD_PREFIX: return std::string("KEYWORD_PREFIX('") + Syntax::KEYWORD_PREFIX_CHAR + "')";
		case TokenType::IDENTIFIER:     return "IDENTIFIER";
		case TokenType::NUMBER:         return "NUMBER";
		case TokenType::STRING:         return "STRING";
		case TokenType::MINUS:          return std::string("MINUS('") + '-' + "')";
		case TokenType::COLON:          return std::string("COLON('") + Syntax::KEYWORD_ASSIGN_CHAR + "')";
		case TokenType::EQUALS:         return std::string("EQUALS('") + Syntax::ASSIGNMENT_CHAR + "')";
		case TokenType::COMMA:          return std::string("COMMA('") + Syntax::SEPARATOR_CHAR + "')";
		case TokenType::SLASH:			return "BACK_SLASH('\\')";
		case TokenType::SEMICOLON:      return std::string("SEMICOLON('") + Syntax::STATEMENT_TERMINATOR + "')";
		case TokenType::BLOCK_BEGIN:    return std::string("BLOCK_BEGIN('") + Syntax::BLOCK_BEGIN_CHAR + "')";
		case TokenType::BLOCK_END:      return std::string("BLOCK_END('") + Syntax::BLOCK_END_CHAR + "')";
		case TokenType::LIST_BEGIN:     return std::string("LIST_BEGIN('") + Syntax::LIST_BEGIN_CHAR + "')";
		case TokenType::LIST_END:       return std::string("LIST_END('") + Syntax::LIST_END_CHAR + "')";
		case TokenType::END_OF_FILE:    return "END_OF_FILE";
		default:                        return "UNKNOWN_TOKEN";
		}
	}

	bool OTNReader::OTNReaderV1::AddError(const Token& token, const std::string error) {
		return AddError("Error at line "
			+ std::to_string(token.line) + ", column "
			+ std::to_string(token.column) + ": "
			+ error);
	}

	bool OTNReader::OTNReaderV1::AddError(const std::string error) {
		if (!m_error.empty())
			m_error += "\n";
		m_error += error;
		m_valid = false;
		return false;
	}

#pragma endregion

	bool OTNReader::OpenFileStream(const OTNFilePath& path) {
		auto& stream = m_readerData.stream;
		stream.open(path, std::ios::in);
		return stream.is_open();
	}

	bool OTNReader::ReadData(std::string_view src, ReaderData& data) {
		OTNTokenizer tokenizer{ src };

		if (!tokenizer.Tokenize()) {
			AddError("Failed to convert data to tokens!");
			AddError(tokenizer.GetError());
			return false;
		}

		auto& tokens = tokenizer.GetTokens();
		if (!SetDataVersion(tokens, data)) {
			AddError("Could not determine file version!");
			return false;
		}

		switch (data.version) {
		case 1: {
			OTNReaderV1 reader{ data, tokens };
			if (!reader.Read()) {
				AddError("Failed to read Tokens!");
				AddError(reader.GetError());
				return false;
			}
			break;
		}
		default:
			AddError("Unsupported OTN version: " + std::to_string(data.version) + "!");
			return false;
		}

		return true;
	}

	bool OTNReader::SetDataVersion(const std::vector<Token>& tokens, ReaderData& data) {
		int fileVersion = -1;

		// file allways has to start with the version tokens
		if (tokens.size() > 4) {
			const Token& tPre = tokens[0];
			const Token& tVers = tokens[1];
			const Token& tCol = tokens[2];
			const Token& tNum = tokens[3];
			const Token& tTerm = tokens[4];

			if (tPre.type == TokenType::KEYWORD_PREFIX &&
				tVers.type == TokenType::IDENTIFIER && tVers.text == Keyword::VERSION_KW &&
				tCol.type == TokenType::COLON &&
				tNum.type == TokenType::NUMBER &&
				tTerm.type == TokenType::SEMICOLON)
			{
				fileVersion = std::stoi(tNum.text);
			}
		}

		data.version = fileVersion;
		return fileVersion > 0;
	}

	void OTNReader::AddError(const std::string& error) {
		if (!m_error.empty())
			m_error += "\n";
		m_error += error;
		m_valid = false;
	}

#pragma endregion

#pragma region OTNStreamWriter

	OTNStreamWriter::~OTNStreamWriter() {
		if (m_file.is_open()) {
			if (m_objectOpen)
				EndObject();
			if (m_blockOpen) {
				m_file.write("};\n", 3);
				m_file.flush();
			}
			m_file.close();
		}
	}

	bool OTNStreamWriter::Open(const OTNFilePath& path) {
		OTNFilePath finalPath;
		std::string pathError;
		if (!ValidateFilePath(path, CREATE_MISSING_DIR, finalPath, pathError)) {
			AddError(pathError);
			return false;
		}

		// Binary mode so that tellp/seekp are byte-accurate (no \r\n translation).
		m_file.open(finalPath, std::ios::binary | std::ios::out | std::ios::trunc
			| std::ios::in /* needed for seekp */);
		if (!m_file.is_open()) {
			AddError("OTNStreamWriter: failed to open '" + finalPath.string() + "'");
			return false;
		}

		// Write version header
		std::string hdr;
		hdr += Syntax::KEYWORD_PREFIX_CHAR;
		hdr += Keyword::VERSION_KW;
		hdr += Syntax::KEYWORD_ASSIGN_CHAR;
		hdr += ' ';
		hdr += std::to_string(static_cast<unsigned>(OTN::VERSION));
		hdr += Syntax::STATEMENT_TERMINATOR;
		hdr += '\n';
		hdr += '\n';

		m_file.write(hdr.data(), static_cast<std::streamsize>(hdr.size()));
		if (!m_file.good()) {
			AddError("OTNStreamWriter: failed to write file header");
			return false;
		}

		m_fileOpen = true;
		return true;
	}

	bool OTNStreamWriter::EnsureObjectBlock() {
		if (m_blockOpen) 
			return true;

		if (!m_fileOpen) { 
			AddError("OTNStreamWriter: file not open"); 
			return false; 
		}

		// @object: {
		std::string blk;
		blk += Syntax::KEYWORD_PREFIX_CHAR;
		blk += Keyword::OBJECT_KW;
		blk += Syntax::KEYWORD_ASSIGN_CHAR;
		blk += ' ';
		blk += Syntax::BLOCK_BEGIN_CHAR;
		blk += '\n';

		m_file.write(blk.data(), static_cast<std::streamsize>(blk.size()));
		if (!m_file.good()) {
			AddError("OTNStreamWriter: failed to write @object block header");
			return false;
		}
		m_blockOpen = true;
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

		if (!EnsureObjectBlock()) 
			return false;

		// Parse type descriptors from strings
		m_currentTypes.clear();
		m_currentTypes.reserve(columnTypes.size());
		for (const auto& t : columnTypes) {
			std::string base = t;
			uint32_t listDepth = 0;
			while (base.size() >= 2 &&
				base[base.size() - 2] == '[' && base.back() == ']') {
				base.resize(base.size() - 2);
				++listDepth;
			}
			OTNTypeDesc desc;
			desc.listDepth = listDepth;
			OTNBaseType bt = StringToOTNBaseType(base);
			if (bt == OTNBaseType::UNKNOWN && !base.empty()) {
				desc.baseType = OTNBaseType::OBJECT;
				desc.refObjectName = base;
			}
			else {
				desc.baseType = bt;
			}
			m_currentTypes.push_back(desc);
		}

		m_columnCount = columnNames.size();
		m_rowCount = 0;
		m_currentObjectName = name;

		// --- Write:  name[NNNNNNNNN] {\n  type/name, ...\n};\n ---
		std::string hdr;
		hdr.reserve(256);
		hdr += name;
		hdr += Syntax::LIST_BEGIN_CHAR;

		m_file.write(hdr.data(), static_cast<std::streamsize>(hdr.size()));
		m_countPos = m_file.tellp(); // remember where the count digits start

		// Placeholder count (COUNT_FIELD_WIDTH zeros)
		std::string placeholder(COUNT_FIELD_WIDTH, '0');
		m_file.write(placeholder.data(), COUNT_FIELD_WIDTH);

		// Build header body: ] {\n  types\n};\n
		std::string body;
		body.reserve(columnNames.size() * 20 + 16);
		body += Syntax::LIST_END_CHAR;
		body += ' ';
		body += Syntax::BLOCK_BEGIN_CHAR;
		body += '\n';
		body += '\t'; // indent for type line

		for (size_t i = 0; i < columnNames.size(); ++i) {
			if (i > 0) { 
				body += Syntax::SEPARATOR_CHAR; 
				body += ' '; 
			}

			const OTNTypeDesc& td = m_currentTypes[i];
			if (!td.refObjectName.empty()) {
				AppendRefName(body, td.refObjectName);
			}
			else {
				body += OTNValueTypeToString(td.baseType);
			}
			for (uint32_t d = 0; d < td.listDepth; ++d)
				body += "[]";

			body += Syntax::TYPE_SEPARATOR_CHAR;
			body += columnNames[i];
		}

		body += '\n';
		body += Syntax::BLOCK_END_CHAR;
		body += Syntax::STATEMENT_TERMINATOR;
		body += '\n';

		m_file.write(body.data(), static_cast<std::streamsize>(body.size()));

		if (!m_file.good()) {
			AddError("OTNStreamWriter: failed to write object header for '" + name + "'");
			return false;
		}

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

		if (argCount != m_columnCount) {
			AddError("OTNStreamWriter: row argument count (" + std::to_string(argCount) +
				") does not match column count (" + std::to_string(m_columnCount) + ")");
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
		std::string rowStr;
		rowStr.reserve(row.size() * 12 + 4);
		rowStr.push_back('\t');
		for (size_t i = 0; i < row.size(); ++i) {
			if (i > 0) {
				rowStr.push_back(Syntax::SEPARATOR_CHAR);
				rowStr.push_back(' ');
			}
			OTNAppendValueToString(rowStr, row[i]);
		}
		rowStr.push_back(Syntax::STATEMENT_TERMINATOR);
		rowStr.push_back('\n');

		m_file.write(rowStr.data(), static_cast<std::streamsize>(rowStr.size()));
		if (!m_file.good()) {
			AddError("OTNStreamWriter: failed to write data row");
			return false;
		}
		m_rowCount++;
		return true;
	}

	bool OTNStreamWriter::EndObject() {
		if (!m_objectOpen) { 
			AddError("OTNStreamWriter: no object open to end"); 
			return false; 
		}
		m_objectOpen = false;

		m_file.flush();

		// Seek back to the placeholder and patch the actual row count
		m_file.seekp(m_countPos);
		if (!m_file.good()) {
			AddError("OTNStreamWriter: seekp failed while patching row count");
			return false;
		}

		std::string countStr = std::to_string(m_rowCount);
		if (static_cast<int>(countStr.size()) > COUNT_FIELD_WIDTH) {
			AddError("OTNStreamWriter: row count exceeds maximum (" +
				std::string(COUNT_FIELD_WIDTH, '9') + ")");
			return false;
		}
		std::string padded(COUNT_FIELD_WIDTH - countStr.size(), '0');
		padded += countStr;
		m_file.write(padded.data(), COUNT_FIELD_WIDTH);

		// Restore write position to end of file
		m_file.seekp(0, std::ios::end);
		if (!m_file.good()) {
			AddError("OTNStreamWriter: seekp to end failed after patching count");
			return false;
		}

		m_currentTypes.clear();
		m_columnCount = 0;
		// Track total rows written for this object name
		m_writtenRowCounts[m_currentObjectName] += m_rowCount;
		m_currentObjectName.clear();
		return true;
	}

	size_t OTNStreamWriter::GetWrittenRowCount(const std::string& objectName) const {
		auto it = m_writtenRowCounts.find(objectName);
		return (it != m_writtenRowCounts.end()) ? it->second : 0;
	}

	bool OTNStreamWriter::WriteObjectHeaderKnown(
		const std::string& name,
		size_t rowCount,
		const std::vector<std::string>& colNames,
		const std::vector<OTNTypeDesc>& colTypes)
	{
		if (!EnsureObjectBlock()) 
			return false;

		std::string hdr;
		hdr.reserve(256 + colNames.size() * 24);

		// name[rowCount] {
		hdr += name;
		hdr += Syntax::LIST_BEGIN_CHAR;
		hdr += std::to_string(rowCount);
		hdr += Syntax::LIST_END_CHAR;
		hdr += ' ';
		hdr += Syntax::BLOCK_BEGIN_CHAR;
		hdr += '\n';
		hdr += '\t';

		for (size_t i = 0; i < colNames.size(); ++i) {
			if (i > 0) { 
				hdr += Syntax::SEPARATOR_CHAR; 
				hdr += ' '; 
			}

			const OTNTypeDesc& td = colTypes[i];
			if (!td.refObjectName.empty()) {
				AppendRefName(hdr, td.refObjectName);
			}
			else {
				hdr += OTNValueTypeToString(td.baseType);
			}

			for (uint32_t d = 0; d < td.listDepth; ++d)
				hdr += "[]";

			hdr += Syntax::TYPE_SEPARATOR_CHAR;
			hdr += colNames[i];
		}

		hdr += '\n';
		hdr += Syntax::BLOCK_END_CHAR;
		hdr += Syntax::STATEMENT_TERMINATOR;
		hdr += '\n';

		m_file.write(hdr.data(), static_cast<std::streamsize>(hdr.size()));
		return m_file.good();
	}

	// Scans a single OTNValue and recursively writes any OTNObjectPtr that has
	// not yet been written.  Works for plain objects and lists of objects.

	bool OTNStreamWriter::EnsureSubObjectsWritten(const OTNValue& val) {
		if (val.type == OTNBaseType::OBJECT) {
			const auto& ptr = std::get<OTNObjectPtr>(val.value);
			if (ptr && m_objPtrToIndex.find(ptr.get()) == m_objPtrToIndex.end()) {
				if (!AppendObject(*ptr)) return false;
			}
		}
		else if (val.type == OTNBaseType::LIST) {
			const auto& arr = std::get<OTNArrayPtr>(val.value);
			if (arr) {
				for (const auto& elem : arr->values) {
					if (!EnsureSubObjectsWritten(elem)) return false;
				}
			}
		}
		return true;
	}

	//   OTNObjectPtr -> int (row index in the target object)
	//   LIST<OBJECT> -> LIST<int>
	//   Everything else is returned unchanged (colType is updated accordingly).

	OTNValue OTNStreamWriter::ConvertValueForStream(
		const OTNValue& val, OTNTypeDesc& colType)
	{
		if (val.type == OTNBaseType::OBJECT) {
			const auto& ptr = std::get<OTNObjectPtr>(val.value);
			if (!ptr) {
				// null object -> index 0
				colType.baseType = OTNBaseType::INT;
				colType.refObjectName.clear();
				return OTNValue(OTNValueVariant(int(0)));
			}
			auto it = m_objPtrToIndex.find(ptr.get());
			int idx = (it != m_objPtrToIndex.end())
				? static_cast<int>(it->second)
				: 0;
			// keep colType.refObjectName so the header still says Ref<XYZ>
			// but the stored value is an int
			return OTNValue(OTNValueVariant(idx));
		}

		if (val.type == OTNBaseType::LIST) {
			const auto& arr = std::get<OTNArrayPtr>(val.value);
			if (arr && !arr->values.empty() &&
				arr->values.front().type == OTNBaseType::OBJECT)
			{
				// Convert list<OTNObjectPtr> → list<int>
				auto newArr = std::make_shared<OTNArray>();
				newArr->values.reserve(arr->values.size());
				for (const auto& elem : arr->values) {
					if (elem.type == OTNBaseType::OBJECT) {
						const auto& eptr = std::get<OTNObjectPtr>(elem.value);
						int idx = 0;
						if (eptr) {
							auto it = m_objPtrToIndex.find(eptr.get());
							if (it != m_objPtrToIndex.end())
								idx = static_cast<int>(it->second);
						}
						newArr->values.emplace_back(OTNValueVariant(idx));
					}
					else {
						newArr->values.push_back(elem);
					}
				}
				// colType stays as LIST of Ref<X> — refObjectName already set
				return OTNValue(OTNValueVariant(newArr));
			}
		}

		return val; // unchanged for all primitive / string / any types
	}

	// AppendObject — collect-first, merge-by-name, depth-first write
	// Design:
	//  1. DFS through the entire object tree, collecting every unique OTNObject*
	//     in post-order (children before parents).
	//  2. Group by object name.  All objects with the same name end up in ONE
	//     file block — their rows are concatenated in DFS discovery order.
	//     This is mandatory because OTNReader only allows one block per name.
	//  3. For every pointer we record its starting row offset so that
	//     ConvertValueForStream can later emit the correct integer index.
	//  4. Write each name-group as a single header + N data rows.

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

		if (!obj.IsValid()) {
			AddError("OTNStreamWriter: object '" + obj.GetObjectName() +
				"' is invalid: " + obj.GetError());
			return false;
		}

		// ---- Step 1: DFS collect ------------------------------------------------
		// post-order list: children appear before parents
		std::vector<const OTNObject*> order;
		std::unordered_set<const OTNObject*> visited;
		// name → objects in discovery order
		std::unordered_map<std::string, std::vector<const OTNObject*>> byName;

		std::function<void(const OTNObject&)> collect = [&](const OTNObject& o) {
			if (visited.count(&o))
				return;
			// Already written and indexed by a prior AppendObject call — skip.
			// Its index is already recorded in m_objPtrToIndex.
			if (m_objPtrToIndex.count(&o))
				return;
			visited.insert(&o);

			for (const auto& row : o.GetDataRows()) {
				for (const auto& val : row) {
					if (val.type == OTNBaseType::OBJECT) {
						const auto& ptr = std::get<OTNObjectPtr>(val.value);
						if (ptr)
							collect(*ptr);
					}
					else if (val.type == OTNBaseType::LIST) {
						const auto& arr = std::get<OTNArrayPtr>(val.value);
						if (!arr)
							continue;

						for (const auto& elem : arr->values) {
							if (elem.type != OTNBaseType::OBJECT)
								continue;

							const auto& eptr = std::get<OTNObjectPtr>(elem.value);
							if (eptr)
								collect(*eptr);
						}
					}
				}
			}

			order.push_back(&o);
			byName[o.GetObjectName()].push_back(&o);
			};
		collect(obj);

		// ---- Step 2: Determine write order (unique names, preserving DFS order) ---
		std::unordered_set<std::string> seenNames;
		std::vector<std::string>        nameOrder;
		for (const auto* o : order) {
			const std::string& n = o->GetObjectName();
			if (seenNames.insert(n).second)
				nameOrder.push_back(n);
		}

		// ---- Step 3: Pre-assign row indices to every pointer ----------------------
		// We must do this BEFORE writing so that ConvertValueForStream works
		// for forward references within the same AppendObject call.
		for (const auto& name : nameOrder) {
			size_t offset = m_writtenRowCounts.count(name) ? m_writtenRowCounts[name] : 0;
			for (const OTNObject* o : byName[name]) {
				m_objPtrToIndex[o] = offset;
				offset += o->GetDataRows().size();
			}
		}

		// ---- Step 4: Write each nam
		// e-group as one merged block --------------------
		// Guard: if any name was already flushed to the file (either by a prior
		// AppendObject or by BeginObject/EndObject), we cannot write a second block
		// with the same name — OTNReader rejects duplicate object names.
		for (const auto& name : nameOrder) {
			if (m_writtenRowCounts.count(name)) {
				AddError("OTNStreamWriter::AppendObject: object type '" + name +
					"' was already written to this file (via a previous AppendObject "
					"or BeginObject/EndObject call). All rows for a given type must "
					"be written in one contiguous block.");
				return false;
			}
		}

		for (const auto& name : nameOrder) {
			const auto& objs = byName[name];

			// Schema from first object in group
			const OTNObject* first = objs.front();
			const auto& colNames = first->GetColumnNames();
			const auto& colTypes = first->GetColumnTypesDesc();

			// Total row count across all objects in this group
			size_t totalRows = 0;
			for (const auto* o : objs)
				totalRows += o->GetDataRows().size();

			// If this name was partially written before (via BeginObject/EndObject),
			// we must open a fresh block — OTN allows multiple same-name blocks only
			// if each is a separate @object directive.  To stay compatible we treat
			// them as continuation.  For simplicity we just write a new block here.
			if (!WriteObjectHeaderKnown(name, totalRows, colNames, colTypes)) {
				AddError("OTNStreamWriter: failed to write header for '" + name + "'");
				return false;
			}

			// Write rows for every object in this group
			for (const OTNObject* o : objs) {
				const auto& rows = o->GetDataRows();
				for (size_t r = 0; r < rows.size(); ++r) {
					const OTNRow& srcRow = rows[r];
					std::string rowStr;
					rowStr.reserve(srcRow.size() * 12 + 4);
					rowStr.push_back('\t');

					for (size_t c = 0; c < srcRow.size(); ++c) {
						if (c > 0) {
							rowStr.push_back(Syntax::SEPARATOR_CHAR);
							rowStr.push_back(' ');
						}
						OTNTypeDesc ct = (c < colTypes.size()) ? colTypes[c] : OTNTypeDesc{};
						OTNValue converted = ConvertValueForStream(srcRow[c], ct);
						OTNAppendValueToString(rowStr, converted);
					}

					rowStr.push_back(Syntax::STATEMENT_TERMINATOR);
					rowStr.push_back('\n');
					m_file.write(rowStr.data(), static_cast<std::streamsize>(rowStr.size()));
					if (!m_file.good()) {
						AddError("OTNStreamWriter: write error in row " +
							std::to_string(r) + " of '" + name + "'");
						return false;
					}
				}
			}

			// Update row count
			m_writtenRowCounts[name] =
				(m_writtenRowCounts.count(name) ? m_writtenRowCounts[name] : 0) + totalRows;
		}

		return m_valid;
	}

	bool OTNStreamWriter::Close() {
		if (!m_fileOpen) return true;

		if (m_objectOpen) {
			if (!EndObject()) return false;
		}

		if (m_blockOpen) {
			m_file.write("};\n", 3);
			m_blockOpen = false;
		}

		m_file.flush();
		m_file.close();
		m_fileOpen = false;
		return m_valid;
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

	void OTNStreamReader::StreamTokenizer::Advance(char c) {
		if (c == '\n') { ++m_line; m_column = 1; }
		else { ++m_column; }
	}

	void OTNStreamReader::StreamTokenizer::AddError(const std::string& msg) {
		if (!m_error.empty()) m_error += '\n';
		m_error += "line " + std::to_string(m_line) + ": " + msg;
		m_valid = false;
	}

	void OTNStreamReader::StreamTokenizer::SkipWhitespace() {
		while (!Eof()) {
			char c = *m_cur;
			if (!std::isspace(static_cast<unsigned char>(c))) break;
			++m_cur;
			Advance(c);
		}
	}

	OTNStreamReader::SRToken OTNStreamReader::StreamTokenizer::ReadString() {
		uint32_t startLine = m_line;
		uint32_t startColumn = m_column;
		
		char c;
		std::string text;
		text.reserve(32);

		bool closed = false;
		while (Get(c)) {
			Advance(c);
			if (c == '"') { 
				closed = true; 
				break; 
			}

			if (c == '\\') {
				if (!Get(c)) 
					break;

				Advance(c);
				switch (c) {
				case 'n':  text += '\n'; break;
				case 't':  text += '\t'; break;
				case '"':  text += '"';  break;
				case '\\': text += '\\'; break;
				default:   AddError("Invalid escape sequence"); return {};
				}
			}
			else {
				text += c;
			}
		}

		if (!closed) {
			AddError("Unterminated string literal"); 
			return {}; 
		}

		return { SRTok::STRING, text, startLine, startColumn };
	}

	OTNStreamReader::SRToken OTNStreamReader::StreamTokenizer::ReadNumber() {
		uint32_t startLine = m_line;
		uint32_t startColumn = m_column;
		
		char c;
		std::string text;
		bool hasDot = false, hasExp = false;
		while (!Eof()) {
			c = *m_cur;
			if (c == '.') {
				if (hasDot || hasExp) { 
					AddError("Invalid number"); 
					return {}; 
				}

				hasDot = true; 
				m_cur++; 
				Advance(c); 
				text += c; 
				continue;
			}

			if (c == 'e' || c == 'E') {
				if (hasExp) { 
					AddError("Invalid number"); 
					return {}; 
				}

				hasExp = true; 
				m_cur++;
				Advance(c);
				text += c;

				int nxt = Peek1();
				if (nxt == '+' || nxt == '-') {
					Get(c); 
					Advance(c); 
					text += c;
				}
				continue;
			}
			if (!std::isdigit(static_cast<unsigned char>(c))) break;
			++m_cur; Advance(c); text += c;
		}
		return { SRTok::NUMBER, text, startLine, startColumn };
	}

	OTNStreamReader::SRToken OTNStreamReader::StreamTokenizer::ReadIdentifier() {
		uint32_t startLine = m_line;
		uint32_t startColumn = m_column;
		
		char c;
		std::string text;
		while (!Eof()) {
			c = *m_cur;
			if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') 
				break;
			
			m_cur++; 
			Advance(c); 
			text += c;
		}

		return { SRTok::IDENTIFIER, text, startLine, startColumn };
	}

	bool OTNStreamReader::StreamTokenizer::Get(char& c) {
		if (m_cur >= m_end) 
			return false;

		c = *m_cur++;
		return true;
	}

	void OTNStreamReader::StreamTokenizer::Unget() { 
		m_cur--; 
	}

	int OTNStreamReader::StreamTokenizer::Peek1() const { 
		return m_cur < m_end ? static_cast<unsigned char>(*m_cur) : -1; 
	}

	bool OTNStreamReader::StreamTokenizer::Eof() const { 
		return m_cur >= m_end; 
	}

	OTNStreamReader::SRToken OTNStreamReader::StreamTokenizer::ReadNextRaw() {
		SkipWhitespace();

		if (Eof())
			return { SRTok::END_OF_FILE, m_line, m_column };

		char c;
		Get(c);
		Advance(c);
		uint32_t tl = m_line, tc = m_column;

		switch (c) {
		case Syntax::STATEMENT_TERMINATOR:
			return { SRTok::SEMICOLON, tl, tc };
		case Syntax::KEYWORD_PREFIX_CHAR:
			return { SRTok::KEYWORD_PREFIX, tl, tc };
		case Syntax::KEYWORD_ASSIGN_CHAR:
			return { SRTok::COLON, tl, tc };
		case Syntax::ASSIGNMENT_CHAR:
			return { SRTok::EQUALS, tl, tc };
		case Syntax::SEPARATOR_CHAR:
			return { SRTok::COMMA, tl, tc };
		case Syntax::TYPE_SEPARATOR_CHAR:
			return { SRTok::SLASH, tl, tc };
		case Syntax::BLOCK_BEGIN_CHAR:
			return { SRTok::BLOCK_BEGIN, tl, tc };
		case Syntax::BLOCK_END_CHAR:
			return { SRTok::BLOCK_END, tl, tc };
		case Syntax::LIST_BEGIN_CHAR:
			return { SRTok::LIST_BEGIN, tl, tc };
		case Syntax::LIST_END_CHAR:
			return { SRTok::LIST_END, tl, tc };
		case Syntax::REF_BEGIN_CHAR:
			return { SRTok::REF_BEGIN, tl, tc };
		case Syntax::REF_END_CHAR:
			return { SRTok::REF_END, tl, tc };
		case '"':
			return ReadString();
		case '-':
			return { SRTok::MINUS, tl, tc };
		default:
			if (std::isdigit(static_cast<unsigned char>(c))) {
				Unget(); 
				m_column--;
				
				return ReadNumber();
			}

			if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
				Unget(); 
				m_column--;
				
				return ReadIdentifier();
			}
			
			AddError(std::string("Unexpected character '") + c + "'");
			return { SRTok::UNKNOWN, std::string(1, c), tl, tc };
		}
	}

	OTNStreamReader::SRToken OTNStreamReader::StreamTokenizer::Next() {
		if (m_peeked.has_value()) {
			SRToken t = std::move(*m_peeked);
			m_peeked.reset();
			return t;
		}

		return ReadNextRaw();
	}

	const OTNStreamReader::SRToken& OTNStreamReader::StreamTokenizer::Peek() {
		if (!m_peeked.has_value())
			m_peeked = ReadNextRaw();

		return *m_peeked;
	}

	bool OTNStreamReader::StreamTokenizer::NextIf(SRTok type) {
		if (Peek().type == type) {
			Next();
			return true;
		}

		return false;
	}

	// ---- OTNStreamReader -------------------------------------------------------

	bool OTNStreamReader::Open(const OTNFilePath& path) {
		OTNFilePath finalPath;
		
		std::string pathError;
		if (!ValidateFilePath(path, !CREATE_MISSING_DIR, finalPath, pathError)) {
			AddError(pathError);
			return false;
		}

		// Slurp file into buffer
		std::ifstream f(finalPath, std::ios::binary | std::ios::ate);
		if (!f.is_open()) {
			AddError("OTNStreamReader: cannot open '" + finalPath.string() + "'");
			return false;
		}
		m_fileBuffer.assign(static_cast<size_t>(f.tellg()), '\0');
		f.seekg(0);
		f.read(m_fileBuffer.data(), static_cast<std::streamsize>(m_fileBuffer.size()));
		f.close();

		m_tok = std::make_unique<StreamTokenizer>(m_fileBuffer);

		if (!ParseFileHeader()) {
			AddError("OTNStreamReader: failed to parse file header");
			return false;
		}
		return true;
	}

	// Parse @version, optional @defType/@defName, then @object: {
	bool OTNStreamReader::ParseFileHeader() {
		bool foundVersion = false;
		bool foundObject = false;

		while (!foundObject) {
			SRToken t = NextTok();
			if (t.type == SRTok::END_OF_FILE) {
				AddError("OTNStreamReader: unexpected end of file in header");
				return false;
			}
			if (t.type != SRTok::KEYWORD_PREFIX) {
				AddTokenError(t, "expected '@'");
				return false;
			}

			SRToken kw = NextTok();
			if (kw.type != SRTok::IDENTIFIER) {
				AddTokenError(kw, "expected keyword identifier after '@'");
				return false;
			}

			if (kw.text == Keyword::VERSION_KW) {
				if (!ParseVersion())
					return false;
				foundVersion = true;
			}
			else if (kw.text == Keyword::DEF_TYPE_KW) {
				if (!ParseDefType())
					return false;
			}
			else if (kw.text == Keyword::DEF_NAME_KW) {
				if (!ParseDefName())
					return false;
			}
			else if (kw.text == Keyword::OBJECT_KW) {
				if (!EnterObjectBlock())
					return false;
				foundObject = true;
			}
			else {
				if (!SkipUntilSemicolon())
					return false;
			}
		}

		if (!foundVersion) {
			AddError("OTNStreamReader: @version directive not found");
			return false;
		}
		return true;
	}

	bool OTNStreamReader::ParseVersion() {
		SRToken colon;
		SRToken num;
		SRToken semi;
		if (!ExpectTok(SRTok::COLON, colon))
			return false;
		if (!ExpectTok(SRTok::NUMBER, num))
			return false;
		if (!ExpectTok(SRTok::SEMICOLON, semi))
			return false;

		try {
			m_version = static_cast<uint8_t>(std::stoi(num.text));
		}
		catch (...) {
			AddTokenError(num, "invalid version number");
			return false;
		}
		return m_valid;
	}

	bool OTNStreamReader::ParseDefType() {
		SRToken dummy;
		if (!ExpectTok(SRTok::COLON, dummy))
			return false;

		do {
			SRToken name;
			SRToken eq;
			SRToken num;
			if (!ExpectTok(SRTok::IDENTIFIER, name))
				return false;
			if (!ExpectTok(SRTok::EQUALS, eq))
				return false;
			if (!ExpectTok(SRTok::NUMBER, num))
				return false;

			if (!m_valid)
				return false;
			try {
				m_defType[static_cast<uint32_t>(std::stoi(num.text))] = name.text;
			}
			catch (...) {
				AddTokenError(num, "invalid defType id");
				return false;
			}
		} while (m_tok->NextIf(SRTok::COMMA));

		SRToken semi;
		return ExpectTok(SRTok::SEMICOLON, semi);
	}

	bool OTNStreamReader::ParseDefName() {
		SRToken dummy;
		if (!ExpectTok(SRTok::COLON, dummy))
			return false;

		do {
			SRToken name;
			SRToken eq;
			SRToken num;
			if (!ExpectTok(SRTok::IDENTIFIER, name))
				return false;
			if (!ExpectTok(SRTok::EQUALS, eq))
				return false;
			if (!ExpectTok(SRTok::NUMBER, num))
				return false;

			if (!m_valid)
				return false;
			try {
				m_defName[static_cast<uint32_t>(std::stoi(num.text))] = name.text;
			}
			catch (...) {
				AddTokenError(num, "invalid defName id");
				return false;
			}
		} while (m_tok->NextIf(SRTok::COMMA));

		SRToken semi;
		return ExpectTok(SRTok::SEMICOLON, semi);
	}

	bool OTNStreamReader::EnterObjectBlock() {
		// Tokens already consumed: @ object
		// Expect: : {
		SRToken colon;
		SRToken beg;
		if (!ExpectTok(SRTok::COLON, colon))
			return false;
		if (!ExpectTok(SRTok::BLOCK_BEGIN, beg))
			return false;

		return m_valid;
	}

	bool OTNStreamReader::NextObject() {
		if (!m_valid)
			return false;
		if (m_atEnd)
			return false;

		// Skip any unread rows from the previous object
		if (m_remainingRows > 0) {
			if (!SkipCurrentObject())
				return false;
		}
		
		// Peek: if we see } the @object block is done
		if (!MoreRemainingObjects()) {
			m_atEnd = true;
			return false;
		}

		return ParseObjectHeader();
	}

	bool OTNStreamReader::ParseObjectHeader() {
		// ObjectName[count] {\n  type/name, ...\n};\n
		SRToken nameToken = NextTok();
		if (nameToken.type != SRTok::IDENTIFIER) {
			AddTokenError(nameToken, "expected object name");
			return false;
		}
		m_currentName = nameToken.text;

		SRToken lb;
		SRToken rb;
		SRToken ct;
		if (!ExpectTok(SRTok::LIST_BEGIN, lb))
			return false;
		if (!ExpectTok(SRTok::NUMBER, ct))
			return false;
		if (!ExpectTok(SRTok::LIST_END, rb))
			return false;
		if (!m_valid)
			return false;

		size_t count = 0;
		try {
			count = std::stoul(ct.text);
		}
		catch (...) {
			AddTokenError(ct, "invalid row count");
			return false;
		}

		// { type/name, ... };
		SRToken beg;
		if (!ExpectTok(SRTok::BLOCK_BEGIN, beg))
			return false;

		m_columnNames.clear();
		m_columnTypes.clear();

		while (m_valid && PeekTok().type != SRTok::BLOCK_END &&
			PeekTok().type != SRTok::END_OF_FILE)
		{
			// --- type part (may be Ref<Name> or identifier/number) ---
			SRToken typeTok = NextTok();
			std::string typeName;
			uint32_t listDepth = 0;

			// Handle Ref<TypeName>
			if (typeTok.type == SRTok::IDENTIFIER && typeTok.text == Keyword::REF_KW) {
				SRToken ltok;
				SRToken rtok;
				SRToken rtok2;
				if (!ExpectTok(SRTok::REF_BEGIN, ltok))
					return false;
				if (!ExpectTok(SRTok::IDENTIFIER, rtok))
					return false;
				if (!ExpectTok(SRTok::REF_END, rtok2))
					return false;

				typeName = rtok.text;
			}
			else if (typeTok.type == SRTok::IDENTIFIER) {
				typeName = typeTok.text;
			}
			else if (typeTok.type == SRTok::NUMBER) {
				// defType substitution
				uint32_t id = static_cast<uint32_t>(std::stoi(typeTok.text));
				auto it = m_defType.find(id);
				if (it == m_defType.end()) {
					AddTokenError(typeTok, "unknown defType id " + typeTok.text);
					return false;
				}
				typeName = it->second;
			}
			else {
				AddTokenError(typeTok, "expected type name");
				return false;
			}

			// Optional [] list suffixes
			while (PeekTok().type == SRTok::LIST_BEGIN) {
				NextTok(); // [
				
				SRToken rb2;
				if (!ExpectTok(SRTok::LIST_END, rb2))
					return false;
				
				listDepth++;
				typeName += "[]";
			}

			SRToken slash;
			if (!ExpectTok(SRTok::SLASH, slash))
				return false;

			// name part (may be identifier or defName number)
			SRToken nameTok = NextTok();
			std::string colName;
			if (nameTok.type == SRTok::IDENTIFIER) {
				colName = nameTok.text;
			}
			else if (nameTok.type == SRTok::NUMBER) {
				uint32_t id = static_cast<uint32_t>(std::stoi(nameTok.text));
				auto it = m_defName.find(id);
				if (it == m_defName.end()) {
					AddTokenError(nameTok, "unknown defName id " + nameTok.text);
					return false;
				}
				colName = it->second;
			}
			else {
				AddTokenError(nameTok, "expected column name");
				return false;
			}

			// Strip the list-depth suffix from typeName before parsing
			std::string baseType = typeName;
			uint32_t extractedDepth = 0;
			while (baseType.size() >= 2 &&
				baseType[baseType.size() - 2] == '[' && baseType.back() == ']') {
				baseType.resize(baseType.size() - 2);
				extractedDepth++;
			}
			// Use max of both depth sources (one from [] tokens, one from string)
			if (extractedDepth > listDepth)
				listDepth = extractedDepth;

			m_columnNames.push_back(colName);
			m_columnTypes.push_back(ParseTypeString(baseType, listDepth));

			NextIfTok(SRTok::COMMA);
		}

		SRToken bend;
		SRToken semi;
		if (!ExpectTok(SRTok::BLOCK_END, bend))
			return false;
		if (!ExpectTok(SRTok::SEMICOLON, semi))
			return false;

		m_remainingRows = count;
		return m_valid;
	}

	bool OTNStreamReader::ReadRow(OTNRow& outRow) {
		if (!m_valid)
			return false;
		if (m_remainingRows == 0)
			return false;

		outRow.clear();
		outRow.reserve(m_columnTypes.size());

		for (size_t col = 0; col < m_columnTypes.size(); ++col) {
			if (col > 0) {
				if (!NextIfTok(SRTok::COMMA)) {
					AddTokenError(PeekTok(), "expected ',' between values");
					return false;
				}
			}
			OTNValue val = ReadValue(m_columnTypes[col]);
			if (!m_valid)
				return false;
			outRow.push_back(std::move(val));
		}

		SRToken semi;
		if (!ExpectTok(SRTok::SEMICOLON, semi))
			return false;

		m_remainingRows--;

		// if no more objects are remaining mark as end
		if (m_remainingRows == 0) {
			if (!MoreRemainingObjects())
				m_atEnd = true;
		}

		return m_valid;
	}

	OTNValue OTNStreamReader::ReadValue(const OTNTypeDesc& td) {
		if (td.baseType == OTNBaseType::ANY) {
			if (td.listDepth > 0) {
				OTNTypeDesc inner{ OTNBaseType::ANY, td.listDepth - 1 };
				return ReadList(inner);
			}
			return ReadAnyValue();
		}

		if (td.listDepth > 0) {
			OTNTypeDesc inner = td;
			inner.listDepth--;
			return ReadList(inner);
		}

		if (td.baseType == OTNBaseType::OBJECT) {
			// Object ref: an integer index
			SRToken tok = NextTok();
			if (tok.type != SRTok::NUMBER) {
				AddTokenError(tok, "expected integer object-ref index");
				return {};
			}

			try {
				return OTNValue(OTNObjectRef(td.refObjectName, std::stoi(tok.text)));
			}
			catch (...) {
				AddTokenError(tok, "invalid object-ref index");
				return {};
			}
		}

		return ReadPrimitive(td.baseType);
	}

	OTNValue OTNStreamReader::ReadAnyValue() {
		const SRToken& peek = PeekTok();
		switch (peek.type) {
		case SRTok::STRING: {
			SRToken t = NextTok();
			return OTNValue(t.text);
		}
		case SRTok::IDENTIFIER: {
			SRToken t = NextTok();
			if (t.text == Keyword::TRUE_KW)
				return OTNValue(true);
			if (t.text == Keyword::FALSE_KW)
				return OTNValue(false);

			AddTokenError(t, "unexpected identifier '" + t.text + "' in 'any' value");
			return {};
		}
		case SRTok::NUMBER: {
			SRToken t = NextTok();
			bool isFloat = t.text.find('.') != std::string::npos
				|| t.text.find('e') != std::string::npos
				|| t.text.find('E') != std::string::npos;

			try {
				return isFloat ? OTNValue(std::stof(t.text)) : OTNValue(std::stoi(t.text));
			}
			catch (...) {
				AddTokenError(t, "invalid numeric literal");
				return {};
			}
		}
		case SRTok::MINUS: {
			NextTok(); // consume -
			SRToken num = NextTok();
			if (num.type != SRTok::NUMBER) {
				AddTokenError(num, "expected number after '-'");
				return {};
			}

			std::string text = "-" + num.text;
			bool isFloat = text.find('.') != std::string::npos
				|| text.find('e') != std::string::npos
				|| text.find('E') != std::string::npos;

			try {
				return isFloat ? OTNValue(std::stof(text)) : OTNValue(std::stoi(text));
			}
			catch (...) {
				AddTokenError(num, "invalid numeric literal");
				return {};
			}
		}
		case SRTok::LIST_BEGIN: {
			OTNTypeDesc inner{ OTNBaseType::ANY, 0 };
			return ReadList(inner);
		}
		default:
			AddTokenError(peek, "unexpected token in 'any' value");
			return {};
		}
	}

	OTNValue OTNStreamReader::ReadList(const OTNTypeDesc& elemType) {
		SRToken lb; 
		if (!ExpectTok(SRTok::LIST_BEGIN, lb))
			return {};
		auto array = std::make_shared<OTNArray>();

		while (m_valid && PeekTok().type != SRTok::LIST_END
			&& PeekTok().type != SRTok::END_OF_FILE)
		{
			OTNValue elem = ReadValue(elemType);
			if (!m_valid)
				return {};

			array->values.push_back(std::move(elem));
			NextIfTok(SRTok::COMMA);
		}

		SRToken rb; 
		if (!ExpectTok(SRTok::LIST_END, rb))
			return {};

		return OTNValue(array);
	}

	OTNValue OTNStreamReader::ReadPrimitive(OTNBaseType bt) {
		switch (bt) {
		case OTNBaseType::INT:
		case OTNBaseType::INT64:
		case OTNBaseType::UINT64:
		case OTNBaseType::FLOAT:
		case OTNBaseType::DOUBLE: {
			bool negative = NextIfTok(SRTok::MINUS);
			SRToken tok = NextTok();
			if (tok.type != SRTok::NUMBER) {
				AddTokenError(tok, "expected numeric literal");
				return {};
			}

			std::string text = negative ? "-" + tok.text : tok.text;
			try {
				switch (bt) {
				case OTNBaseType::INT:    return OTNValue(std::stoi(text));
				case OTNBaseType::INT64:  return OTNValue(static_cast<int64_t>(std::stoll(text)));
				case OTNBaseType::UINT64: return OTNValue(static_cast<uint64_t>(std::stoull(text)));
				case OTNBaseType::FLOAT:  return OTNValue(std::stof(text));
				case OTNBaseType::DOUBLE: return OTNValue(std::stod(text));
				default: break;
				}
			}
			catch (...) {
				AddTokenError(tok, "invalid numeric literal");
			}

			return {};
		}
		case OTNBaseType::BOOL: {
			SRToken tok = NextTok();
			if (tok.type == SRTok::IDENTIFIER) {
				if (tok.text == Keyword::TRUE_KW)
					return OTNValue(true);
				if (tok.text == Keyword::FALSE_KW)
					return OTNValue(false);
			}
			else if (tok.type == SRTok::NUMBER) {
				return OTNValue(tok.text != "0");
			}

			AddTokenError(tok, "expected boolean literal");
			return {};
		}
		case OTNBaseType::STRING: {
			SRToken tok = NextTok();
			if (tok.type != SRTok::STRING) { 
				AddTokenError(tok, "expected string literal"); 
				return {}; 
			}

			return OTNValue(tok.text);
		}
		default:
			AddTokenError(PeekTok(), "unsupported primitive type");
			return {};
		}
	}

	bool OTNStreamReader::SkipCurrentObject() {
		while (m_valid && m_remainingRows > 0) {
			if (!SkipUntilSemicolon())
				return false;
			m_remainingRows--;
		}

		return m_valid;
	}

	bool OTNStreamReader::SkipUntilSemicolon() {
		size_t depth = 0;
		while (m_valid) {
			SRToken t = NextTok();
			if (t.type == SRTok::END_OF_FILE) {
				AddError("OTNStreamReader: unexpected end of file while skipping");
				return false;
			}
			if (t.type == SRTok::LIST_BEGIN || t.type == SRTok::BLOCK_BEGIN)
				depth++;

			if (t.type == SRTok::LIST_END || t.type == SRTok::BLOCK_END) {
				if (depth)
					depth--;
			}

			if (t.type == SRTok::SEMICOLON && depth == 0)
				return true;
		}
		return false;
	}

	bool OTNStreamReader::MoreRemainingObjects() {
		const SRToken& peek = PeekTok();
		return !(peek.type == SRTok::BLOCK_END || peek.type == SRTok::END_OF_FILE);
	}

	OTN::OTNTypeDesc OTNStreamReader::ParseTypeString(
		const std::string& baseTypeName, uint32_t listDepth) const
	{
		OTNTypeDesc desc;
		desc.listDepth = listDepth;
		OTNBaseType bt = StringToOTNBaseType(baseTypeName);
		if (bt == OTNBaseType::UNKNOWN && !baseTypeName.empty()) {
			desc.baseType = OTNBaseType::OBJECT;
			desc.refObjectName = baseTypeName;
		}
		else {
			desc.baseType = bt;
		}

		return desc;
	}

	OTNStreamReader::SRToken OTNStreamReader::NextTok() { 
		return m_tok->Next(); 
	}

	const OTNStreamReader::SRToken& OTNStreamReader::PeekTok() { 
		return m_tok->Peek(); 
	}

	bool OTNStreamReader::NextIfTok(SRTok t) { 
		return m_tok->NextIf(t); 
	}

	bool OTNStreamReader::ExpectTok(SRTok type, SRToken& out) {
		out = NextTok();
		if (out.type != type) {
			AddTokenError(out, "unexpected token '" + out.text + "'");
			return false;
		}

		return m_valid;
	}

	bool OTNStreamReader::ExpectIdentifier(const std::string& text) {
		SRToken t = NextTok();
		if (t.type != SRTok::IDENTIFIER || t.text != text) {
			AddTokenError(t, "expected '" + text + "'");
			return false;
		}

		return m_valid;
	}

	bool OTNStreamReader::HasMoreObjects() const {
		return m_valid && !m_atEnd;
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

	uint8_t OTNStreamReader::GetVersion() const {
		return m_version;
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

	void OTNStreamReader::AddError(const std::string& msg) {
		if (!m_error.empty())
			m_error.push_back('\n');
		m_error += msg;
		m_valid = false;
	}

	void OTNStreamReader::AddTokenError(const SRToken& tok, const std::string& msg) {
		AddError("line " + std::to_string(tok.line) + ":" +
			std::to_string(tok.column) + ": " + msg);
	}

#pragma endregion

}