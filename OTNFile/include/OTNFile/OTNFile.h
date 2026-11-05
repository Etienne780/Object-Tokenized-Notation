#pragma once
#include <vector>
#include <array>
#include <fstream>
#include <string>
#include <variant>
#include <memory>
#include <optional>
#include <filesystem>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <charconv>
#include <iomanip>
#include <cmath>
#include <limits>

/**
* @brief OTN_DEBUG enables additional checks that cost time for every row
* (types of every row, unique names in custom types).
*
* Default: on in debug builds (without NDEBUG), off otherwise.
* Define it as 0 or 1 in the build system (the same value for all files) to override it.
*/
#ifndef OTN_DEBUG
	#ifdef NDEBUG
		#define OTN_DEBUG 0
	#else
		#define OTN_DEBUG 1
	#endif
#endif

/**
* @file OTNFile.h
* @brief OTN (Object Tokenized Notation) - A file format library for structured data serialization
*
* OTN is a text-based file format for storing object data in a tabular style with type safety.
* It supports primitive types (int, float, double, bool, string), nested objects,
* and multi-dimensional arrays.
*
* @section usage Basic Usage
*
* **Writing data:**
* @code
OTN::OTNObject obj("MyData");
obj.SetNames("id", "name", "value")
	.AddDataRow(1, "Item1", 42.5)
	.AddDataRow(2, "Item2", 17.3);

OTN::OTNWriter writer;
writer.AppendObject(obj)
	.Save("data.otn");
* @endcode
*
* **Reading data:**
* @code
OTN::OTNReader reader;
if (reader.ReadFile("data.otn")) {
	auto obj = reader.TryGetObject("MyData");
	if (obj) {
		int id = obj->GetValue<int>(0, "id");
		std::string name = obj->GetValue<std::string>(0, "name");
		// Or
		int id = obj->GetValue<int>(0, "id", 0);
		std::string name = obj->GetValue<std::string>(0, "name", "defaultVal");
		// Or
		std::optional<int> id = obj->TryGetValue<int>(0, "id");
		std::optional<std::string> name = obj->TryGetValue<std::string>(0, "name");

	}
}
* @endcode
*
* @section custom_types Custom Types
*
* This section explains how to define serialization and deserialization
* for custom types using template specialization.
*
* Notes:
* - Objects must provide a default constructor.
* - Loading uses the order in which elements are added.
*   Changing this order will affect save/load compatibility.
*
* Basic example:
* @code
struct Vector2 { float x, y; };

template<>
inline void OTN::ToOTNDataType<Vector2>(OTN::OTNObjectBuilder& obj, Vector2& v) {
	obj.SetObjectName("Vector2");
	obj.AddNames("x", "y");
	obj.AddData(v.x, v.y);
}
* @endcode
*
* Advanced example using IsBuildingOTNObject():
* This allows independent control over save and load behavior.
*
* @code
template<>
inline void OTN::ToOTNDataType<Vector2>(OTN::OTNObjectBuilder& obj, Vector2& e) {
	obj.SetObjectName("Vector2");
	obj.AddNames("list");

	if (obj.IsBuildingOTNObject()) {
		// Serialization
		obj.AddData(std::vector<float>{ e.x, e.y });
	} else {
		// Deserialization
		std::vector<float> tmp;
		obj.AddData(tmp);

		if (tmp.size() < 2)
			return;

		e.x = tmp[0];
		e.y = tmp[1];
	}
}
* @endcode
*
* @version 2.0
*/

/**
* @brief Object Tokenized Notation - A structured data serialization library
*
* The OTN namespace provides classes and utilities for:
* 
* - Creating structured tabular data (OTNObject)
* 
* - Serializing to .otn files (OTNWriter)
* 
* - Deserializing from .otn files (OTNReader)
* 
* - Custom type support via template specialization
*
* section thread_safety Thread Safety
* 
* All classes are NOT thread-safe. Use external synchronization.
*
* section exceptions Exception Safety
* 
* - Most functions provide basic exception guarantee
* 
* - GetValue<T>() may throw std::out_of_range or std::bad_cast
* 
* - File operations may throw std::filesystem exceptions
*/
namespace OTN {

	/// @brief Current version of the OTN format.
	inline constexpr uint8_t VERSION = 2;

	/// @brief Standard file extension for OTN files.
	inline constexpr std::string_view FILE_EXTENSION = ".otn";

	/// @brief File extension of temporary files created by OTNStreamWriter next to the target file.
	inline constexpr std::string_view TEMP_FILE_EXTENSION = ".otn.tmp";

	namespace Syntax {

		inline constexpr char STATEMENT_TERMINATOR = ';';
		inline constexpr char KEYWORD_PREFIX_CHAR = '@';
		inline constexpr char KEYWORD_ASSIGN_CHAR = ':';
		inline constexpr char ASSIGNMENT_CHAR = '=';
		inline constexpr char SEPARATOR_CHAR = ',';
		inline constexpr char TYPE_SEPARATOR_CHAR = '/';
		inline constexpr char BLOCK_BEGIN_CHAR = '{';
		inline constexpr char BLOCK_END_CHAR = '}';
		inline constexpr char LIST_BEGIN_CHAR = '[';
		inline constexpr char LIST_END_CHAR = ']';
		inline constexpr char REF_BEGIN_CHAR = '<';
		inline constexpr char REF_END_CHAR = '>';
		inline constexpr char REF_INDEX_CHAR = '#'; // Name#index, reference in an 'any' value (version 2)

		namespace Types {
			inline constexpr std::string_view INT = "int";
			inline constexpr std::string_view INT64 = "int64";
			inline constexpr std::string_view UINT64 = "uint64";
			inline constexpr std::string_view FLOAT = "float";
			inline constexpr std::string_view DOUBLE = "double";
			inline constexpr std::string_view BOOL = "bool";
			inline constexpr std::string_view STRING = "String";
			inline constexpr std::string_view OBJECT = "object";
			inline constexpr std::string_view OBJECT_REF = "object_ref";
			inline constexpr std::string_view LIST = "list";
			inline constexpr std::string_view ANY = "any";
		}
	}

	namespace Keyword {

		inline constexpr std::string_view VERSION_KW = "version";
		inline constexpr std::string_view DEF_TYPE_KW = "defType";
		inline constexpr std::string_view DEF_NAME_KW = "defName";

		inline constexpr std::string_view OBJECT_KW = "object";
		inline constexpr std::string_view REF_KW = "Ref";

		inline constexpr std::string_view TRUE_KW = "true";
		inline constexpr std::string_view FALSE_KW = "false";

	}

	/// @brief Type alias for file system paths
	using OTNFilePath = std::filesystem::path;

	/// @brief Enum representing the fundamental data types supported by OTN.
	enum class OTNBaseType {
		UNKNOWN = 0,
		INT,
		UINT64,
		INT64,
		FLOAT,
		DOUBLE,
		BOOL,
		STRING,
		OBJECT,
		OBJECT_REF,
		LIST,
		ANY
	};

	namespace Detail {

		template<typename>
		inline constexpr bool otn_always_false_v = false;

	}

	class OTNObjectBuilder;

	/**
	* @brief Template for converting between T and OTNObject.
	*
	* note:
	*
	* - Must be specialized for user-defined types.
	*
	* @tparam T Type to serialize/deserialize.
	*/
	template<typename T>
	void ToOTNDataType(OTNObjectBuilder&, T&) {
		static_assert(Detail::otn_always_false_v<T>, "Unsupported type for ToOTNDataType");
	}

	/**
	* @brief Type descriptor with support for nested arrays
	*
	* Describes a data type including base type, nesting depth for arrays,
	* and optional object type name for custom types.
	*
	* Examples:
	*
	* - int: baseType=INT, listDepth=0
	*
	* - int[]: baseType=INT, listDepth=1
	*
	* - int[][]: baseType=INT, listDepth=2
	*
	* - CustomObj: baseType=OBJECT, refObjectName="CustomObj"
	*/
	struct OTNTypeDesc {
		OTNBaseType baseType = OTNBaseType::UNKNOWN;
		uint32_t listDepth = 0;
		std::string refObjectName; // only valid if baseType == OBJECT

		OTNTypeDesc() = default;

		/**
		* @brief Construct type descriptor for primitive or list type
		* @param type Base data type
		* @param depth Array nesting depth (default: 0)
		*/
		OTNTypeDesc(OTNBaseType type, uint32_t depth = 0)
			: baseType(type), listDepth(depth) {
		}

		/**
		* @brief Construct type descriptor for custom object type
		* @param type Object type name
		* @param depth Array nesting depth (default: 0)
		*/
		OTNTypeDesc(const std::string& type, uint32_t depth = 0)
			: baseType(OTNBaseType::OBJECT), listDepth(depth), refObjectName(type) {
		}
	};

	/// @brief Internal types and helpers, not meant to be used directly.
	namespace Detail {

		/**
		* @brief Internal reference to an object instance
		* Used during deserialization to resolve object references
		*/
		struct OTNObjectRef {
			std::string refObjectName;
			size_t index = 0;

			OTNObjectRef() = default;

			/**
			* @brief Construct object reference
			* @param _objName Name of referenced object type
			* @param _index Instance index
			*/
			OTNObjectRef(const std::string& _objName, size_t _index)
				: refObjectName(_objName), index(_index) {
			}

			bool operator==(const OTNObjectRef& other) const {
				return index == other.index && refObjectName == other.refObjectName;
			}
		};

	}

	/**
	* @brief Convert string to OTNBaseType
	* @param type Type name (e.g., "int", "float", "String")
	* @return Corresponding OTNBaseType or UNKNOWN if not recognized
	*/
	OTNBaseType StringToOTNBaseType(const std::string& type);

	/**
	* @brief Convert OTNBaseType to string representation
	* @param type Base type enum value
	* @return String representation (e.g., "int", "float")
	*/
	constexpr std::string_view OTNValueTypeToString(OTNBaseType type) noexcept {
		namespace SynTypes = Syntax::Types;

		switch (type) {
		case OTNBaseType::INT:        return SynTypes::INT;
		case OTNBaseType::INT64:      return SynTypes::INT64;
		case OTNBaseType::UINT64:     return SynTypes::UINT64;
		case OTNBaseType::FLOAT:      return SynTypes::FLOAT;
		case OTNBaseType::DOUBLE:     return SynTypes::DOUBLE;
		case OTNBaseType::BOOL:       return SynTypes::BOOL;
		case OTNBaseType::STRING:     return SynTypes::STRING;
		case OTNBaseType::OBJECT:     return SynTypes::OBJECT;
		case OTNBaseType::OBJECT_REF: return SynTypes::OBJECT_REF;
		case OTNBaseType::LIST:       return SynTypes::LIST;
		case OTNBaseType::ANY:        return SynTypes::ANY;
		case OTNBaseType::UNKNOWN:
		default:                      return "UNKNOWN";
		}
	}

	/**
	* @brief Get character length of type name
	* @param type Base type enum value
	* @return Length of type name string
	*/
	constexpr uint32_t OTNValueTypeCharLength(OTNBaseType type) noexcept {
		return static_cast<uint32_t>(OTNValueTypeToString(type).size());
	}

	/**
	* @brief Convert type descriptor to string representation
	* @param type Type descriptor
	* @return String like "int", "float[][]", or "CustomType"
	*/
	std::string TypeDescToString(const OTNTypeDesc& type);

	// -------------------------------------------------------------------------
	// Column definition helper — used with the variadic BeginObject overload
	// -------------------------------------------------------------------------

	/**
	* @brief Describes one column: its name and its type string.
	*
	* Type strings are the same as used in SetTypes() on OTNObject, e.g.:
	* "int", "float", "String", "bool", "int[]", "MyObject", "MyObject[]".
	*
	* Example:
	* @code
	* w.BeginObject("Weapons",
	*     OTN::col("id",     "int"),
	*     OTN::col("name",   "String"),
	*     OTN::col("damage", "float"));
	* @endcode
	*/
	struct OTNColumnDef {
		std::string name;
		std::string type;

		OTNColumnDef(std::string n, std::string t)
			: name(std::move(n)), type(std::move(t)) {
		}

		OTNColumnDef(const char* n, const char* t)
			: name(n), type(t) {
		}
	};

	/**
	* @brief Convenience factory — creates an OTNColumnDef from name and type string.
	*/
	inline OTNColumnDef col(std::string name, std::string type) {
		return { std::move(name), std::move(type) };
	}

#pragma region OTNObject

	class OTNObject;

	namespace Detail {
		class OTNArray;

		using OTNObjectPtr = std::shared_ptr<OTNObject>;
		using OTNArrayPtr = std::shared_ptr<OTNArray>;
	}

	using OTNValueVariant = std::variant<
		int,
		int64_t,
		uint64_t,
		float,
		double,
		bool,
		std::string,
		Detail::OTNObjectPtr,
		Detail::OTNArrayPtr,
		Detail::OTNObjectRef
	>;

	namespace Detail {

		template<typename T>
		struct OTNTypeOf {
			static constexpr OTNBaseType value = OTNBaseType::UNKNOWN;
		};

		template<> struct OTNTypeOf<int> {
			static constexpr OTNBaseType value = OTNBaseType::INT;
		};

		template<> struct OTNTypeOf<int64_t> {
			static constexpr OTNBaseType value = OTNBaseType::INT64;
		};

		template<> struct OTNTypeOf<uint64_t> {
			static constexpr OTNBaseType value = OTNBaseType::UINT64;
		};

		template<> struct OTNTypeOf<float> {
			static constexpr OTNBaseType value = OTNBaseType::FLOAT;
		};

		template<> struct OTNTypeOf<double> {
			static constexpr OTNBaseType value = OTNBaseType::DOUBLE;
		};

		template<> struct OTNTypeOf<bool> {
			static constexpr OTNBaseType value = OTNBaseType::BOOL;
		};

		template<> struct OTNTypeOf<std::string> {
			static constexpr OTNBaseType value = OTNBaseType::STRING;
		};

		template<> struct OTNTypeOf<Detail::OTNObjectPtr> {
			static constexpr OTNBaseType value = OTNBaseType::OBJECT;
		};

		template<> struct OTNTypeOf<Detail::OTNObjectRef> {
			static constexpr OTNBaseType value = OTNBaseType::OBJECT_REF;
		};

		template<> struct OTNTypeOf<Detail::OTNArrayPtr> {
			static constexpr OTNBaseType value = OTNBaseType::LIST;
		};

		template<typename T>
		inline constexpr OTNBaseType GetType() {
			return OTNTypeOf<std::decay_t<T>>::value;
		}

		inline OTNBaseType GetTypeFromVariant(const OTNValueVariant& variant) {
			return std::visit(
				[](const auto& value) constexpr -> OTNBaseType {
					using T = std::decay_t<decltype(value)>;
					return GetType<T>();
				},
				variant
			);
		}

	}

	/**
	* @brief Container for a single data value of any supported type
	*/
	struct OTNValue {
		OTNValueVariant value;
		OTNBaseType type = OTNBaseType::UNKNOWN;

		OTNValue() = default;

		/**
		* @brief Construct OTNValue from variant
		* @param v Value variant
		*/
		explicit OTNValue(OTNValueVariant v)
			: value(std::move(v)), type(Detail::GetTypeFromVariant(value)) {
		}

		OTNValue(const OTNValue& other)
			: value(other.value), type(other.type) {
		}

		OTNValue(OTNValue&& other) noexcept
			: value(std::move(other.value)), type(other.type)
		{
			other.type = OTNBaseType::UNKNOWN;
		}

		OTNValue& operator=(const OTNValue& other) {
			if (this != &other) {
				value = other.value;
				type = other.type;
			}
			return *this;
		}

		OTNValue& operator=(OTNValue&& other) noexcept {
			if (this != &other) {
				value = std::move(other.value);
				type = other.type;
				other.type = OTNBaseType::UNKNOWN;
			}
			return *this;
		}

		bool operator==(const OTNValue& other) const {
			if (type != other.type)
				return false;

			return std::visit(
				[](const auto& a, const auto& b) -> bool {
					using T = std::decay_t<decltype(a)>;

					if constexpr (!std::is_same_v<T, std::decay_t<decltype(b)>>) {
						return false;
					}
					else if constexpr (
						std::is_same_v<T, Detail::OTNObjectPtr> ||
						std::is_same_v<T, Detail::OTNArrayPtr>)
					{
						return a.get() == b.get();
					}
					else {
						return a == b;
					}
				},
				value,
				other.value
			);
		}
	};

	namespace Detail {

		/**
		* @brief Container for array/list values
		* Used internally to represent lists
		*/
		class OTNArray {
		public:
			std::vector<OTNValue> values;
		};

		template<typename T>
		struct otn_is_std_vector : std::false_type {};

		template<typename U, typename Alloc>
		struct otn_is_std_vector<std::vector<U, Alloc>> : std::true_type {};

		template<typename T>
		static constexpr bool is_otn_variant_v =
			std::is_same_v<std::decay_t<T>, OTNValueVariant>;

		template<typename T>
		struct is_otn_base_type : std::false_type {};

		template<> struct is_otn_base_type<int> : std::true_type {};
		template<> struct is_otn_base_type<int64_t> : std::true_type {};
		template<> struct is_otn_base_type<uint64_t> : std::true_type {};
		template<> struct is_otn_base_type<float> : std::true_type {};
		template<> struct is_otn_base_type<double> : std::true_type {};
		template<> struct is_otn_base_type<bool> : std::true_type {};
		template<> struct is_otn_base_type<std::string> : std::true_type {};
		template<> struct is_otn_base_type<const char*> : std::true_type {};
		template<size_t N> struct is_otn_base_type<const char[N]> : std::true_type {};
		template<> struct is_otn_base_type<Detail::OTNObjectRef> : std::true_type {};

		template<typename T>
		inline constexpr bool is_otn_base_type_v = is_otn_base_type<T>::value;

		template<typename T>
		struct is_otn_list : std::false_type {};

		template<typename T, typename Alloc>
		struct is_otn_list<std::vector<T, Alloc>> : std::true_type {};
		template<typename T, std::size_t N>
		struct is_otn_list<std::array<T, N>> : std::true_type {};

		template<typename T>
		inline constexpr bool is_otn_list_v = is_otn_list<T>::value;

		template<typename T>
		inline constexpr bool is_otn_number_v =
			std::is_arithmetic_v<T> && !std::is_same_v<T, bool>;

		/**
		* @brief Range checked conversion between numeric types.
		* @return std::nullopt if the value does not fit into To (or is not integral for integer targets)
		*/
		template<typename To, typename From>
		std::optional<To> OTNNumericCast(From value) {
			if constexpr (std::is_floating_point_v<To>) {
				return static_cast<To>(value);
			}
			else if constexpr (std::is_floating_point_v<From>) {
				if (!std::isfinite(value) || std::trunc(value) != value)
					return std::nullopt;

				constexpr long double lo = static_cast<long double>(std::numeric_limits<To>::min());
				constexpr long double hi = static_cast<long double>(std::numeric_limits<To>::max()) + 1.0L;
				const long double v = static_cast<long double>(value);
				if (!(v >= lo && v < hi))
					return std::nullopt;
				return static_cast<To>(value);
			}
			else if constexpr (std::is_signed_v<From> == std::is_signed_v<To>) {
				if (value < std::numeric_limits<To>::min() || value > std::numeric_limits<To>::max())
					return std::nullopt;
				return static_cast<To>(value);
			}
			else if constexpr (std::is_signed_v<From>) {
				if (value < 0 || static_cast<std::make_unsigned_t<From>>(value) > std::numeric_limits<To>::max())
					return std::nullopt;
				return static_cast<To>(value);
			}
			else {
				if (value > static_cast<std::make_unsigned_t<To>>(std::numeric_limits<To>::max()))
					return std::nullopt;
				return static_cast<To>(value);
			}
		}

		/**
		* @brief Converts any numeric value stored in a variant to To.
		* @return std::nullopt if the variant holds no number or the value does not fit.
		*/
		template<typename To>
		std::optional<To> OTNConvertNumber(const OTNValueVariant& variant) {
			return std::visit([](const auto& v) -> std::optional<To> {
				using From = std::decay_t<decltype(v)>;
				if constexpr (Detail::is_otn_number_v<From>)
					return Detail::OTNNumericCast<To>(v);
				else
					return std::nullopt;
				}, variant);
		}

	}

	// Type alias for a row of data values
	using OTNRow = std::vector<OTNValue>;

	/**
	* @brief Represents a structured data object in the OTN format.
	*
	* An OTNObject works like a table or a dataframe. It has a name, a set of column names,
	* column types, and a list of rows containing data.
	* It is the primary data structure for manipulating data in memory before saving or after loading.
	*
	* notes:
	*
	* - Column names must be set before adding data rows.
	*
	* - Types can be auto-deduced from the first data row if not explicitly set.
	*
	* - Object can become invalid while build use IsValid, GetError or TryGetError for more information
	*/
	class OTNObject {
		friend class OTNObjectBuilder;
	public:
		/**
		* @brief Construct a new OTN object with given name
		*
		* note:
		*
		* - Name must not start with a digit and must not contain special characters
		*
		* @param name Unique identifier for this object type
		*/
		explicit OTNObject(const std::string& name);

		OTNObject(const OTNObject& other);
		OTNObject& operator=(const OTNObject& other);

		OTNObject(OTNObject&& other) noexcept;
		OTNObject& operator=(OTNObject&& other) noexcept;

		~OTNObject() = default;

		/**
		* @brief Set column names using variadic arguments
		*
		* note:
		*
		* - Must be called before AddDataRow()
		*
		* - Names must not contain special characters or start with digits
		*
		* @tparam Args String-convertible types
		* @param names Column names (must be unique)
		* @return Reference to this object for method chaining
		*/
		template<typename ...Args>
		OTNObject& SetNames(Args&&... names) {
			static_assert((std::is_convertible_v<Args, std::string> && ...),
				"All names must be convertible to std::string");

			if (!m_dataRows.empty()) {
				AddError(
					"SetNames must be called before AddData in object '" + m_name + "'!"
				);
				return *this;
			}
			auto addName = [&](const std::string& n) {
				if (IsNameValid(n))
					m_columnNames.emplace_back(n);
				else
					AddError("Name '" + n + "' contains invalid characters");
				};

			constexpr size_t ArgCount = sizeof...(Args);
			m_columnIndexCache.clear();
			m_columnNames.clear();
			m_columnNames.reserve(ArgCount);

			(addName(std::forward<Args>(names)), ...);

			ValidateNamesDistinct();
			return *this;
		}

		/**
		* @brief Set column names from a vector
		*
		* note:
		*
		* - Must be called before AddDataRow()
		*
		* @param names Vector of column names
		* @return Reference to this object for method chaining
		*/
		OTNObject& SetNamesList(const std::vector<std::string>& names);

		/**
		* @brief Explicitly sets the data types for the columns.
		*
		* If this function is not called, column types are deduced from
		* the first row of data added.
		*
		* Supported type strings include for example:
		* "int", "int64", "uint64", "float", "double", "bool", "String", "any", "objectName", "int[]".
		*
		* Use "-", "", or "_" to indicate skip the definition of this type.
		* Type will automaticly be deduced if a row is inserted.
		*
		* notes:
		*
		* - This function must be called before AddData().
		*       Calling it after data has been added will result in an error.
		*
		* - On failure, this object will contain an error state.
		*       More information can be retrieved via GetError() or TryGetError().
		*
		* @tparam Args Variadic list of type strings.
		* @param types One type string per column.
		*
		* @return Reference to this object for method chaining
		*/
		template<typename ...Args>
		OTNObject& SetTypes(Args&&... types) {
			static_assert((std::is_convertible_v<Args, std::string> && ...),
				"All types must be convertible to std::string");

			if (!m_dataRows.empty()) {
				AddError(
					"SetTypes must be called before AddData in object '" + m_name + "'!"
				);
				return *this;
			}
			constexpr size_t ArgCount = sizeof...(Args);

			std::vector<OTNTypeDesc> tmp;
			tmp.reserve(ArgCount);

			auto addType = [&](const std::string& t) {
				return AddSingleType(tmp, t);
				};

			if (!(addType(std::forward<Args>(types)) && ...)) {
				return *this; // Do not modify m_types on failure
			}

			m_columnTypes = std::move(tmp);
			return *this;
		}

		/**
		* @brief Explicitly sets the data types for the columns.
		*
		* If this function is not called, column types are deduced from
		* the first row of data added.
		*
		* Supported type strings include:
		* "int", "int64", "uint64", "float", "double", "bool", "String", "any", "objectName", "int[]".
		*
		* Use "-", "", or "_" to indicate skip the definition of this type.
		* Type will automaticly be deduced if a row is inserted.
		*
		* notes:
		*
		* - This function must be called before AddData().
		*       Calling it after data has been added will result in an error.
		*
		* - On failure, this object will contain an error state.
		*       More information can be retrieved via GetError() or TryGetError().
		*
		* @param types List of type strings, one entry per column.
		*
		* @return Reference to this object for method chaining
		*/
		OTNObject& SetTypesList(const std::vector<std::string>& types);

		/**
		* @brief Sets the column types using a list of OTNTypeDesc objects.
		*
		* Explicitly defines the data type of each column by providing
		* a fully constructed OTNTypeDesc per column.
		*
		* Unlike SetTypesList(), no parsing or string conversion is performed.
		* The provided descriptors are assigned directly.
		*
		* notes:
		*
		* - This function must be called before AddData().
		*       Calling it after data has been added will result in an error.
		*
		* - On failure, this object will contain an error state.
		*       More information can be retrieved via GetError() or TryGetError().
		*
		* @param types List of OTNTypeDesc objects, one entry per column.
		*
		* @return Reference to this object to allow method chaining.
		*/
		OTNObject& SetTypeDescList(const std::vector<OTNTypeDesc>& types);

		/**
		* @brief Add a data row using variadic arguments
		*
		* notes:
		*
		* - Number of arguments must match number of columns
		*
		* - First row is used for type deduction if types weren't explicitly set
		*
		* @tparam Args Value types (must match column types)
		* @param args Values for each column
		* @return Reference to this object for method chaining
		*/
		template<typename... Args>
		OTNObject& AddDataRow(Args&&... args) {
			constexpr size_t ArgCount = sizeof...(Args);

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

			if (ArgCount != m_columnNames.size()) {
				AddError("AddDataRow argument count (" + std::to_string(ArgCount) +
					") does not match name count (" +
					std::to_string(m_columnNames.size()) +
					") in object '" + m_name + "'!");
				return *this;
			}

			OTNRow row;
			row.reserve(ArgCount);

			bool rowValid = true;
			auto addValue = [&](auto&& value) {
				AddSingleValueToRow(row, std::forward<decltype(value)>(value), rowValid);
				};
			(addValue(std::forward<Args>(args)), ...);

			if (!rowValid)
				return *this;

			m_dataRows.emplace_back(std::move(row));

			if (!m_deducedColumnTypes) {
				if (!DeduceTypesFromRow(m_dataRows.back()))
					return *this;
			}

#if OTN_DEBUG
			ValidateRowTypes(m_dataRows.size() - 1);
#endif
			return *this;
		}

		/**
		* @brief Add a data row using a OTNROW
		*
		* notes:
		*
		* - Number of arguments must match number of columns
		*
		* - First row is used for type deduction if types weren't explicitly set
		*
		* @param values Values for each column
		* @return Reference to this object for method chaining
		*/
		OTNObject& AddDataRowList(const OTNRow& values);

		/**
		* @brief Pre-allocate storage for data rows
		* @param amount Number of rows to reserve space for
		* @return Reference to this object for method chaining
		*/
		OTNObject& ReserveDataRows(size_t amount);

		/**
		* @brief Check if object is in a valid state
		* @return true if no errors occurred during construction
		*/
		bool IsValid() const;

		/*
		* @brief checks if this OTNObjects was build from a type T
		* @return true if this was build from a type T
		*/
		bool WasBuildFromT() const;

		/**
		* @brief Try to retrieve error message
		* @param[out] outError Error message if any
		* @return true if an error exists
		*/
		bool TryGetError(std::string& outError) const;

		/**
		* @brief Get accumulated error messages
		* @return Error message string (empty if no errors)
		*/
		std::string GetError() const;

		/**
		* @brief Get the objects name
		* @return Object name/identifier
		*/
		std::string GetObjectName() const;

		/*
		* @brief Sets the name of the object
		* @return true if name was set
		*/
		bool SetObjectName(const std::string& name);

		/**
		* @brief Returns the number of columns based on the defined column names.
		*
		* This reflects the logical column count of the object schema,
		*
		* independent of any stored data rows.
		*
		* @return Number of columns defined by column names.
		*/
		size_t GetColumnCount() const;

		/**
		* @brief Returns the number of columns for a specific data row.
		*
		* This reflects the actual number of values stored in the given row.
		*
		* @param rowIndex Index of the data row.
		* @return Number of columns in the specified row.
		*
		* @throws std::out_of_range if the row index is invalid.
		*/
		size_t GetColumnCount(size_t rowIndex) const;

		/**
		* @brief Get number of data rows
		* @return Row count
		*/
		size_t GetRowCount() const;

		/**
		* @brief Get column names (read-only)
		* @return Const reference to column names vector
		*/
		const std::vector<std::string>& GetColumnNames() const;

		/**
		* @brief Get column type descriptors (read-only)
		* @return Const reference to column types vector
		*/
		const std::vector<OTNTypeDesc>& GetColumnTypesDesc() const;

		/**
		* @brief Get all data rows (read-only)
		* @return Const reference to data rows vector
		*/
		const std::vector<OTNRow>& GetDataRows() const;

		/**
		* @brief Get column names (mutable)
		*
		* WARNING: Modifying names after adding data may cause inconsistencies
		*
		* @return Reference to column names vector
		*/
		std::vector<std::string>& GetColumnNames();

		/**
		* @brief Get column type descriptors (mutable)
		*
		* WARNING: Modifying types after adding data may cause inconsistencies
		*
		* @return Reference to column types vector
		*/
		std::vector<OTNTypeDesc>& GetColumnTypesDesc();

		/**
		* @brief Get all data rows (mutable)
		* @return Reference to data rows vector
		*/
		std::vector<OTNRow>& GetDataRows();

		/**
		* @brief Get a specific row (read-only)
		* @param index Row index (0-based)
		* @return Const reference to row
		* @throws std::out_of_range if index is invalid
		*/
		const OTNRow& GetRow(size_t index) const;

		/**
		* @brief Get a specific row (mutable)
		* @param index Row index (0-based)
		* @return Reference to row
		* @throws std::out_of_range if index is invalid
		*/
		OTNRow& GetRow(size_t index);

		/**
		* @brief Get a typed value by row and column name
		* @tparam T Expected value type
		* @param row Row index (0-based)
		* @param columnName Name of the column (column to index name will be cached)
		* @return Deserialized value of type T
		* @throws std::out_of_range if row or column doesn't exist
		* @throws std::bad_cast if type conversion fails
		*/
		template<typename T>
		T GetValue(size_t row, const std::string& columnName) const {
			auto colId = GetColumnID(columnName);
			if (!colId)
				throw std::out_of_range("column not found");

			return GetValue<T>(row, *colId);
		}

		/**
		* @brief Get a typed value by row and column index
		* @tparam T Expected value type
		* @param row Row index (0-based)
		* @param column Column index (0-based)
		* @return Deserialized value of type T
		* @throws std::out_of_range if row or column doesn't exist
		* @throws std::bad_cast if type conversion fails
		*/
		template<typename T>
		T GetValue(size_t row, size_t column) const {
			if (row >= m_dataRows.size())
				throw std::out_of_range("row out of bounds");

			const OTNRow& r = m_dataRows[row];
			if (column >= r.size())
				throw std::out_of_range("column out of bounds");

			auto opt = TryDeserializeValueImpl<T>(r[column], IsAnyColumn(column));
			if (!opt)
				throw std::bad_cast();

			return *opt;
		}

		/**
		* @brief Get a typed value with default fallback by row and column name
		* @tparam T Expected value type
		* @param row Row index
		* @param objName Column name
		* @param defaultValue Value to return if retrieval fails
		* @return Retrieved value or defaultValue
		*/
		template<typename T>
		T GetValue(size_t row, const std::string& objName, const T& defaultValue) const {
			auto colId = GetColumnID(objName);
			if (!colId)
				return defaultValue;

			auto opt = TryGetValue<T>(row, *colId);
			return (opt) ? *opt : defaultValue;
		}

		/**
		* @brief Get a typed value with default fallback by row and column index
		* @tparam T Expected value type
		* @param row Row index
		* @param column Column index
		* @param defaultValue Value to return if retrieval fails
		* @return Retrieved value or defaultValue
		*/
		template<typename T>
		T GetValue(size_t row, size_t column, const T& defaultValue) const {
			auto opt = TryGetValue<T>(row, column);
			return (opt) ? *opt : defaultValue;
		}

		/**
		* @brief Try to get a typed value by row and column name
		* @tparam T Expected value type
		* @param row Row index
		* @param columnName Column name
		* @return std::optional containing value if successful, std::nullopt otherwise
		*/
		template<typename T>
		std::optional<T> TryGetValue(size_t row, const std::string& columnName) const {
			auto colId = GetColumnID(columnName);
			if (!colId)
				return std::nullopt;

			return TryGetValue<T>(row, *colId);
		}

		/**
		* @brief Try to get a typed value by row and column index
		* @tparam T Expected value type
		* @param row Row index
		* @param column Column index
		* @return std::optional containing value if successful, std::nullopt otherwise
		*/
		template<typename T>
		std::optional<T> TryGetValue(size_t row, size_t column) const {
			if (row >= m_dataRows.size())
				return std::nullopt;

			const OTNRow& r = m_dataRows[row];
			if (column >= r.size())
				return std::nullopt;

			return TryDeserializeValueImpl<T>(r[column], IsAnyColumn(column));
		}

		/**
		* @brief Internal helper to convert an OTNValue variant into a C++ type T.
		*
		* Supports:
		*
		* - Primitive types (int, float, double, bool, string)
		*
		* - Vectors/lists (including nested lists)
		*
		* - Custom objects via ToOTNDataType specialization
		*
		* @tparam T The target type
		* @param val The source OTNValue
		* @return std::optional with the converted value, or std::nullopt on failure
		*/
		template<typename T>
		std::optional<T> TryDeserializeValue(const OTNValue& val) const {
			return TryDeserializeValueImpl<T>(val, false);
		}

	private:
		std::string m_name;
		mutable std::string m_error;
		mutable bool m_valid = true;
		bool m_wasBuildFromT = false;

		bool m_deducedColumnTypes = false;
		std::vector<std::string> m_columnNames;
		std::vector<OTNTypeDesc> m_columnTypes;
		std::vector<OTNRow> m_dataRows;
		mutable std::unordered_map<std::string, size_t> m_columnIndexCache;

		/**
		* @brief Returns the column index for a given column name.
		*
		* Searches the list of defined column names and returns the index
		* of the first matching entry.
		*
		* @param name Column name to search for.
		* @return Column index if found, otherwise std::nullopt.
		*/
		std::optional<size_t> GetColumnID(const std::string& name) const;

		// true if the column type is 'any', numbers in such columns can be read as any numeric type
		bool IsAnyColumn(size_t column) const;

		/**
		* @brief Converts an OTNValue into T, see TryDeserializeValue.
		* @param allowNumericConversion If true, numbers are converted (range checked) to the requested numeric type.
		*/
		template<typename T>
		std::optional<T> TryDeserializeValueImpl(const OTNValue& val, bool allowNumericConversion) const;

		/**
		* @brief Adds a single value to an output row with type validation.
		*
		* This function accepts either:
		*
		* - An already constructed OTNValue
		*
		* - A supported OTN variant type
		*
		* - A native type that can be wrapped in OTNDataType
		*
		* On failure, the row is marked as invalid and will no be added, also an error is reported.
		*
		* @tparam U Type of the value to add.
		* @param outRow Target row where the value will be appended.
		* @param value Value to add.
		* @param outRowValid Set to false if validation fails.
		*/
		template<typename U>
		void AddSingleValueToRow(OTNRow& outRow, U&& value, bool& outRowValid);

		void AddError(const std::string& error) const;
		void SetBuildFromT(bool value);
		void SetNamesFromBuilder(std::vector<std::string>&& names);
		bool SetTypesFromBuilder(const std::vector<std::string>& types);
		void AddRowInternal(OTNRow&& row);
		bool AddSingleType(std::vector<OTNTypeDesc>& tempList, const std::string& typeStr);
		bool DeduceTypesFromRow(const OTNRow& row);

		// object, column and type names: letters, digits, '_' and UTF-8 chars, not starting with a digit
		static bool IsNameValid(const std::string& name);
		// validates if each column name is distinct
		bool ValidateNamesDistinct();
		// validates if each value of the row matches the type of its column
		bool ValidateRowTypes(size_t rowIndex);
	};

	/**
	* @brief Helper class for serializing and deserializing objects to/from OTNObjects.
	*
	* This class operates in two modes:
	*
	* 1. Building an OTNObject from an object of type T (serialization).
	*
	* 2. Building an object of type T from an existing OTNObject (deserialization).
	*
	* The mode is determined by the constructor used.
	*/
	class OTNObjectBuilder {
	public:
		/**
		* @brief Default constructor for building an OTNObject from T.
		*/
		OTNObjectBuilder() = default;

		/**
		* @brief Constructor for building an object of type T from an existing OTNObject.
		* @param obj Source OTNObject to deserialize from.
		*/
		OTNObjectBuilder(const OTNObject& obj);
		~OTNObjectBuilder() = default;

		/**
		* @brief Sets the name of the object being built.
		* @param name Name of the object.
		*
		* In serialization mode, sets the object name.
		*
		* In deserialization mode, checks for name consistency and reports an error if it differs.
		*/
		void SetObjectName(const std::string& name);

		/**
		* @brief Adds multiple column names to the object.
		* @tparam Args Variadic list of column name types convertible to std::string.
		* @param names Column names to add.
		* @return Reference to self for method chaining.
		*/
		template<typename... Args>
		OTNObjectBuilder& AddNames(Args&&... names) {
			static_assert((std::is_convertible_v<Args, std::string> && ...),
				"All names must be convertible to std::string");

			if (m_otnObjectFromT) {
				(AddName(std::forward<Args>(names)) && ...);
			}
			return *this;
		}

		/**
		* @brief Adds multiple data values corresponding to the columns.
		* @tparam Args Variadic list of values to add.
		* @param args Data values.
		* @return Reference to self for method chaining.
		*/
		template<typename... Args>
		OTNObjectBuilder& AddData(Args&&... args) {
			(AddSingleData(std::forward<Args>(args)) && ...);
			return *this;
		}

		/**
		* @brief Adds a single column name and its corresponding value.
		*
		* Checks that names and data are in sync. Reports errors if not.
		*
		* @tparam T Type of the value.
		* @param name Name of the column.
		* @param data Value to add.
		* @return Reference to self for chaining.
		*/
		template<typename T>
		OTNObjectBuilder& AddNameData(const std::string& name, T&& data) {
			if (!m_otnObjectFromT) {
				AddSingleData(std::forward<T>(data));
				return *this;
			}

			if (IsDataOutOfSync()) {
				AddError(
					"Error adding Name and Data (AddNameData): data and names are not in sync (" +
					std::to_string(m_data.size()) + " != " +
					std::to_string(m_dataNames.size()) + ") in object '" +
					m_objectName + "'!"
				);
				return *this;
			}

			if (!AddName(name)) {
				return *this;
			}

			AddSingleData(std::forward<T>(data));
			return *this;
		}

		/**
		* @brief Sets the types of the next columns, like OTNObject::SetTypes.
		*
		* Without types they are deduced from the values. Use "-" to deduce the type of a single column.
		* Types are needed for empty lists and 'any' columns. Ignored when loading.
		*
		* @tparam Args Variadic list of type strings convertible to std::string.
		* @return Reference to self for method chaining.
		*/
		template<typename... Args>
		OTNObjectBuilder& AddTypes(Args&&... types) {
			static_assert((std::is_convertible_v<Args, std::string> && ...),
				"All types must be convertible to std::string");

			if (m_otnObjectFromT)
				(m_dataTypes.emplace_back(std::forward<Args>(types)), ...);
			return *this;
		}

		/**
		* @brief Adds a single column with name, type and value.
		*
		* Like AddNameData, the type is used like a type of AddTypes.
		*
		* @tparam T Type of the value.
		* @param name Name of the column.
		* @param type Type string of the column, see OTNObject::SetTypes.
		* @param data Value to add.
		* @return Reference to self for chaining.
		*/
		template<typename T>
		OTNObjectBuilder& AddNameTypeData(const std::string& name, const std::string& type, T&& data) {
			if (m_otnObjectFromT) {
				if (m_dataTypes.size() > m_dataNames.size()) {
					AddError("Error adding Name, Type and Data (AddNameTypeData): more types than names in object '" +
						m_objectName + "'!");
					return *this;
				}
				// columns added without a type are deduced
				m_dataTypes.resize(m_dataNames.size());
				m_dataTypes.push_back(type);
			}
			return AddNameData(name, std::forward<T>(data));
		}

		/**
		* @brief Returns true if the builder is in serialization mode (T → OTNObject).
		*/
		bool IsBuildingOTNObject() const;

		/**
		* @brief Returns true if the builder is valid (no errors encountered).
		*/
		bool IsValid() const;

		/**
		* @brief Returns the current error string.
		*/
		std::string GetError() const;

		/**
		* @brief Converts the builder into an OTNObject (serialization).
		* @return Constructed OTNObject.
		* @throws std::runtime_error if called in deserialization mode.
		*/
		OTNObject ToOTNObject()&&;

	private:
		bool m_otnObjectFromT = true;/* determins in which mode the object build ist, build OTNObject from t or build t from OTNObject*/
		bool m_valid = true;
		std::string m_error;
		std::string m_objectName;

		std::vector<std::string> m_dataNames;
		std::vector<std::string> m_dataTypes; // types of AddTypes / AddNameTypeData, can be shorter than m_dataNames
		std::vector<bool> m_anyColumns; // loading: columns of type 'any', their numbers can be read as every number type
		std::vector<OTNValue> m_data;
		size_t m_dataPos = 0;/*< is used when T is set from OTNBuilder to detrimend from which position the data should come from*/

		void AddError(const std::string& error);

		/**
		* @brief Adds a single column name to the builder.
		* @param name Name to add.
		* @return True if the name was successfully added (unique in serialization mode).
		*/
		bool AddName(std::string name);

		/**
		* @brief Adds a single data value to the row, handling serialization or deserialization.
		* @tparam T Type of the value.
		* @param data Value to add.
		* @return True if the operation succeeded.
		*/
		template<typename T>
		bool AddSingleData(T&& data);

		/**
		* @brief Deserializes an OTNValue into the provided variable.
		* @tparam T Target type.
		* @param val Source OTNValue.
		* @param data Target variable.
		* @return True if deserialization succeeded.
		*/
		template<typename T>
		bool DeserializeFromOTNValue(const OTNValue& val, T& data, bool allowNumericConversion = false);

		/**
		* @brief Checks if a name is unique in the current object.
		* @param name Name to check.
		* @return True if the name is unique.
		*/
		bool IsNameUnique(const std::string& name) const;

		/**
		* @brief Checks if the number of column names matches the number of data values.
		* @return True if data is out of sync with column names.
		*/
		bool IsDataOutOfSync() const;
	};

	namespace Detail {

		/**
		* @brief Template for converting a value of type U into an OTNValue.
		* @tparam U Type to wrap.
		*
		* Handles:
		* 
		* - Base types
		* 
		* - Lists (vectors)
		* 
		* - Nested objects
		*/
		template<typename U>
		class OTNDataType {
		public:
			// accept both lvalues and rvalues
			explicit OTNDataType(const U& value) {
				Construct(std::forward<const U&>(value));
			}

			explicit OTNDataType(U&& value) {
				Construct(std::forward<U>(value));
			}

		private:
			void Construct(const U& value) {
				using DT = std::decay_t<U>;

				if constexpr (is_otn_list_v<DT>) {
					Detail::OTNArrayPtr otnArray = std::make_shared<Detail::OTNArray>();
					otnArray->values.reserve(value.size());
					for (auto&& elem : value) {
						OTNDataType<std::decay_t<decltype(elem)>> elemData(elem);
						if (!elemData.m_valid) {
							AddError(elemData.m_error);
							return;
						}
						otnArray->values.emplace_back(std::move(elemData.m_value));
					}
					m_value = otnArray;
				}
				else if constexpr (std::is_same_v<DT, OTNObject>) {
					m_value = std::make_shared<OTNObject>(value); // copy
				}
				else if constexpr (std::is_same_v<DT, const char*>) {
					m_value = std::string(value);
				}
				else if constexpr (is_otn_base_type_v<DT>) {
					m_value = value;
				}
				else if constexpr (std::is_same_v<DT, uint32_t> ||
					std::is_same_v<DT, unsigned int>) {
					m_value = static_cast<int64_t>(value);
				}
				else {
					OTNObjectBuilder builder;
					// cast const away, because ToOTNDataType needs a non-const reference (value is only read while serializing)
					ToOTNDataType<DT>(builder, const_cast<U&>(value));
					if (!builder.IsValid()) {
						AddError(builder.GetError());
						return;
					}
					Detail::OTNObjectPtr ptr = std::make_shared<OTNObject>(std::move(builder).ToOTNObject());

					if (!ptr->IsValid())
						AddError(ptr->GetError());

					m_value = ptr;
				}
			}

		public:
			OTNValueVariant m_value;
			bool m_valid = true;
			std::string m_error;

			void AddError(const std::string& error) {
				m_valid = false;
				if (!m_error.empty())
					m_error += "\n";
				m_error += error;
			}
		};

	}

	template<typename T>
	std::optional<T> OTNObject::TryDeserializeValueImpl(const OTNValue& val, bool allowNumericConversion) const {
		using DT = std::decay_t<T>;

		if constexpr (Detail::is_otn_base_type_v<DT>) {
			if (const DT* v = std::get_if<DT>(&val.value))
				return *v;

			if constexpr (Detail::is_otn_number_v<DT>) {
				if (allowNumericConversion)
					return Detail::OTNConvertNumber<DT>(val.value);
			}
			return std::nullopt;
		}
		else if constexpr (std::is_same_v<DT, uint32_t> || std::is_same_v<DT, unsigned int>) {
			// stored as int64 by OTNDataType
			if (const int64_t* v = std::get_if<int64_t>(&val.value))
				return Detail::OTNNumericCast<DT>(*v);

			if (allowNumericConversion)
				return Detail::OTNConvertNumber<DT>(val.value);
			return std::nullopt;
		}
		else if constexpr (Detail::otn_is_std_vector<DT>::value) {
			if (val.type != OTNBaseType::LIST)
				return std::nullopt;

			Detail::OTNArrayPtr ptr = std::get<Detail::OTNArrayPtr>(val.value);
			if (!ptr)
				return std::nullopt;

			DT result{};
			using ElemType = typename DT::value_type;
			result.reserve(ptr->values.size());
			for (const OTNValue& elem : ptr->values) {
				auto conv = TryDeserializeValueImpl<ElemType>(elem, allowNumericConversion);
				if (!conv)
					return std::nullopt;
				result.push_back(std::move(*conv));
			}
			return result;
		}
		else if constexpr (std::is_same_v<DT, OTNObject>) {
			if (val.type != OTNBaseType::OBJECT)
				return std::nullopt;

			try {
				Detail::OTNObjectPtr ptr = std::get<Detail::OTNObjectPtr>(val.value);
				if (!ptr)
					return std::nullopt;

				return *ptr;
			}
			catch (...) {
				return std::nullopt;
			}
		}
		else {
			if (val.type != OTNBaseType::OBJECT)
				return std::nullopt;

			Detail::OTNObjectPtr ptr = std::get<Detail::OTNObjectPtr>(val.value);
			if (!ptr || ptr->GetRowCount() != 1)
				return std::nullopt;

			OTNObjectBuilder builder{ *ptr };
			T result{};
			ToOTNDataType<DT>(builder, result);
			if (!builder.IsValid()) {
				AddError(builder.GetError());
				return std::nullopt;
			}
			return result;
		}
	}

	template<typename U>
	void OTNObject::AddSingleValueToRow(OTNRow& outRow, U&& value, bool& outRowValid) {
		using DT = std::decay_t<U>;
		if constexpr (std::is_same_v<DT, OTNValue>) {
			outRow.emplace_back(std::forward<U>(value));
		}
		else if constexpr (Detail::is_otn_variant_v<DT>) {
			outRow.emplace_back(std::forward<U>(value));
		}
		else {
			Detail::OTNDataType<DT> data(std::forward<U>(value));
			if (!data.m_valid) {
				outRowValid = false;
				AddError("Invalid data in object '" + m_name + "':\n" + data.m_error);
				return;
			}
			outRow.emplace_back(std::move(data.m_value));
		}
	}

	template<typename T>
	bool OTNObjectBuilder::AddSingleData(T&& data) {
		using U = std::decay_t<T>;

		if (m_otnObjectFromT) {
			// Serialize: T -> OTNObject
			Detail::OTNDataType<U> wrappedData(std::forward<T>(data));
			if (!wrappedData.m_valid) {
				AddError("OTNObjectBuilder: failed to serialize data: " + wrappedData.m_error);
				return false;
			}
			m_data.emplace_back(std::move(wrappedData.m_value));
		}
		else {
			// Deserialize: OTNObject -> T
			if constexpr (std::is_const_v<std::remove_reference_t<T>>) {
				AddError("OTNObjectBuilder: cannot deserialize into a const value");
				return false;
			}
			else {
				if (m_dataPos >= m_data.size()) {
					AddError("OTNObjectBuilder: insufficient data to extract value");
					return false;
				}
				const bool anyColumn = m_dataPos < m_anyColumns.size() && m_anyColumns[m_dataPos];
				OTNValue& val = m_data[m_dataPos++];
				if (!DeserializeFromOTNValue<U>(val, data, anyColumn)) {
					AddError("OTNObjectBuilder: failed to deserialize value");
					return false;
				}
			}
		}

		return true;
	}

	template<typename T>
	bool OTNObjectBuilder::DeserializeFromOTNValue(const OTNValue& val, T& data, bool allowNumericConversion) {
		using DT = std::decay_t<T>;

		if constexpr (Detail::is_otn_base_type_v<DT>) {
			// Base type
			if (const DT* v = std::get_if<DT>(&val.value)) {
				data = *v;
				return true;
			}

			if constexpr (Detail::is_otn_number_v<DT>) {
				if (allowNumericConversion) {
					if (std::optional<DT> converted = Detail::OTNConvertNumber<DT>(val.value)) {
						data = *converted;
						return true;
					}
				}
			}
			return false;
		}
		else if constexpr (std::is_same_v<DT, uint32_t> || std::is_same_v<DT, unsigned int>) {
			// stored as int64 by OTNDataType
			std::optional<DT> converted;
			if (const int64_t* v = std::get_if<int64_t>(&val.value))
				converted = Detail::OTNNumericCast<DT>(*v);
			else if (allowNumericConversion)
				converted = Detail::OTNConvertNumber<DT>(val.value);

			if (!converted)
				return false;

			data = *converted;
			return true;
		}
		else if constexpr (std::is_same_v<DT, OTNObject>) {
			// Custom object
			if (val.type != OTNBaseType::OBJECT)
				return false;
			const Detail::OTNObjectPtr& ptr = std::get<Detail::OTNObjectPtr>(val.value);
			if (!ptr)
				return false;
			data = *(ptr.get());
			return true;
		}
		else if constexpr (Detail::is_otn_list_v<DT>) {
			// List or nested list
			data.clear();
			using ElemType = typename DT::value_type;

			std::vector<OTNValue> elements;
			if (val.type == OTNBaseType::LIST) {
				Detail::OTNArrayPtr arrayPtr = std::get<Detail::OTNArrayPtr>(val.value);
				if (!arrayPtr)
					return false;
				elements = arrayPtr->values;
			}
			else {
				elements.push_back(val); // single value treated as list
			}

			for (auto& elemVal : elements) {
				ElemType elem{};
				if (!DeserializeFromOTNValue<ElemType>(elemVal, elem, allowNumericConversion))
					return false;
				data.push_back(std::move(elem));
			}
			return true;
		}
		else {
			// other types are custom types, they are made with the object builder
			if (val.type != OTNBaseType::OBJECT)
				return false;

			Detail::OTNObjectPtr ptr = std::get<Detail::OTNObjectPtr>(val.value);
			if (!ptr)
				return false;

			if (ptr->GetRowCount() > 1)
				return false;

			OTNObjectBuilder builder{ *ptr };
			ToOTNDataType<DT>(builder, data);
			if (!builder.IsValid()) {
				AddError(builder.GetError());
				return false;
			}
			return true;
		}
	}

#pragma endregion

	namespace Detail {

		// position in an OTN file, line/column are kept for error messages
		struct FilePosition {
			uint64_t offset = 0;
			uint32_t line = 1;
			uint32_t column = 1;
		};

		// ids of type or column names, defined with @defType / @defName
		class DefTable {
		public:
			void Add(uint32_t id, const std::string& name);
			void Clear();
			bool Empty() const;
			const std::string* FindName(uint32_t id) const;
			std::optional<uint32_t> FindId(const std::string& name) const;
			// all entries sorted by id
			std::vector<std::pair<uint32_t, std::string>> GetEntries() const;

		private:
			std::unordered_map<uint32_t, std::string> m_names;
			std::unordered_map<std::string, uint32_t> m_ids;
		};

		// everything in front of the @object block
		struct FileHeaderInfo {
			uint8_t version = VERSION;
			DefTable defType;
			DefTable defName;
		};

		// header of one object: Name[rowCount] { type/name, ... };
		struct ObjectHeaderInfo {
			std::string name;
			size_t rowCount = 0;
			std::vector<std::string> columnNames;
			std::vector<OTNTypeDesc> columnTypes;
		};

		// formatting of written files
		struct WriteOptions {
			bool compact = false; // no spaces, line breaks and indentation
		};

		// defined in OTNFile.cpp
		class CharSource;
		class Tokenizer;
		struct ReadSyntax;

	}

#pragma region OTNWriter

	/**
	* @brief Serializes and writes OTNObjects to a file.
	*
	* This class provides fine-grained control over the writing process,
	* including optimizations, name/type definition usage, and row deduplication.
	*/
	class OTNWriter {
	public:
		/**
		* @brief Default constructor.
		*/
		explicit OTNWriter() = default;
		/**
		* @brief Destructor.
		*/
		~OTNWriter() = default;

		/**
		* @brief Replace frequently used strings with numeric IDs to save space.
		* @param value True to enable, false to store full strings.
		* @return Reference to self for chaining.
		*/
		OTNWriter& UseDefName(bool value);

		/**
		* @brief Replace frequently used type names with numeric IDs to save space.
		* @param value True to enable, false to store full type names.
		* @return Reference to self for chaining.
		*/
		OTNWriter& UseDefType(bool value);

		/**
		* @brief Enable or disable optimizations (removes spaces, line breaks).
		* @param value True to enable, false to disable.
		* @return Reference to self for method chaining.
		*/
		OTNWriter& UseOptimizations(bool value);

		/**
		* @brief Enable or disable deduplication of identical rows.
		* @param value True to enable, false to disable.
		* @return Reference to self for method chaining.
		*/
		OTNWriter& UseDeduplicateRows(bool value);

		/**
		* @brief Append an OTNObject to the writer.
		* @param object Object to append.
		* @return Reference to self for method chaining.
		*
		* In debug mode, ensures no duplicate object names exist.
		*/
		OTNWriter& AppendObject(const OTNObject& object);

		/**
		* @brief Remove all OTNObjects from the writer.
		* @return Reference to self for method chaining.
		*
		* Clears the internal object container. Capacity is not reduced.
		*/
		OTNWriter& ClearObjects();

		/**
		* @brief Removes all objects and the error state, the options stay.
		* @return Reference to self for method chaining.
		*
		* Use it to write another file with the same writer, also after an error.
		*/
		OTNWriter& Reset();

		/**
		* @brief Reserve storage for a specified number of OTNObjects.
		* @param amount Number of objects to reserve capacity for.
		* @return Reference to self for method chaining.
		*
		* Adjusts the capacity of the internal container to accommodate
		* at least the specified number of objects without reallocation.
		*/
		OTNWriter& ReserveObjects(size_t amount);

		/**
		* @brief Save all appended objects to a file.
		*
		* Validates the objects and path, writes the data, and stores any errors internally.
		*
		* @param path Absolute file path including file name (e.g., "file.otn" or "file").
		*             Parent directories will be created automatically if they do not exist.
		* @return True if saving succeeded, false otherwise. Retrieve more information via GetError() or TryGetError()
		*/
		bool Save(const OTNFilePath& path);

		/**
		* @brief Serialize all appended objects into an OTN string.
		*
		* @param outText Receives the serialized OTN text on success.
		* @return True if serialization succeeded, false otherwise.
		*/
		bool SaveToString(std::string& outText);

		/**
		* @brief Returns whether name definitions are used.
		*/
		bool GetUseDefName() const;

		/**
		* @brief Returns whether type definitions are used.
		*/
		bool GetUseDefType() const;

		/**
		* @brief Returns whether optimizations are enabled.
		*/
		bool GetUseOptimizations() const;

		/**
		* @brief Returns whether row deduplication is enabled.
		*/
		bool GetDeduplicateRows() const;

		/**
		* @brief Returns true if the writer is valid (no errors occurred).
		*/
		bool IsValid() const;

		/**
		* @brief Attempts to get the current error message.
		* @param outError String to store the error.
		* @return True if an error exists, false if the writer is valid.
		*/
		bool TryGetError(std::string& outError) const;

		/**
		* @brief Returns the current error message string.
		*/
		std::string GetError() const;

	private:
		struct SerializedObject {
			using Row = std::vector<OTNValue>;
			std::vector<std::string> columnNames;
			std::vector<OTNTypeDesc> columnTypes;

			// uses map to add only unique rows and vector for stable indices
			std::vector<Row> rows;
			std::unordered_map<std::size_t, std::size_t> rowIndexByHash;

			// Adds row if not already present, returns index
			size_t AddOrGetRow(const Row& row, bool deduplicateRows);
		private:
			static size_t CreateRowHash(const std::vector<OTNTypeDesc>& columnTypes, const Row& row);
			// static size_t HashValue(const SerializedValue& serValue);
			static size_t HashValue(const OTNTypeDesc& colType, const OTNValue& value);
		};

		struct WriterData {
			bool created = false;
			std::ofstream stream;  // target file, not open when writing to a string
			std::string buffer;    // written text, flushed into stream when it gets large

			std::unordered_map<OTNBaseType, uint32_t> typeUsage;// < map contains which data types are used and how often
			std::unordered_map<std::string, uint32_t> objectColumnUsage;// < how often object names are used as column type (Ref<X>)
			std::unordered_map<std::string, uint32_t> objectRefUsage;// < how often object names are used in references of 'any' columns
			std::unordered_map<std::string, SerializedObject> objects;

			Detail::FileHeaderInfo fileHeader;// < version and the @defType / @defName ids used for optimizations

			void Reset() {
				if (stream.is_open())
					stream.close();
				buffer.clear();
				created = false;
				typeUsage.clear();
				objectColumnUsage.clear();
				objectRefUsage.clear();
				objects.clear();
				fileHeader = {};
			}
		};

		static constexpr size_t FLUSH_SIZE = 64 * 1024; // 64 KB

		bool m_useDefName = false;// < replaces often used names with number
		bool m_useDefType = false;// < replaces often used type names with numbers
		bool m_useOptimizations = false;// < (Removes spaces, linebreaks)
		bool m_useDeduplicateRows = false;

		std::vector<OTNObject> m_objects;
		std::string m_error;
		bool m_valid = true;

		WriterData m_writerData;

		// checks that all objects are valid and have distinct names
		bool ValidateObjects();

		bool WriteToFile(const OTNFilePath& path);
		bool WriteToString(std::string& outText);
		bool CreateWriteData(WriterData& data);
		std::vector<size_t> AddObject(WriterData& data, OTNObject& object);
		void ConvertToSerValue(WriterData& data, OTNValue& result, OTNTypeDesc& colType, const OTNValue& val);
		bool CreateDefType();
		bool CreateDefName();

		bool WriteHeader();
		bool WriteBody();
		// writes the buffer into the file if it is large enough (or force is set)
		bool FlushBuffer(bool force);
		Detail::WriteOptions GetWriteOptions() const;

		void AddError(const std::string& error);

		void CountObjectType(const SerializedObject& obj, WriterData& data);
		static OTNTypeDesc DeduceColumnType(const OTNValue& value);
	};

#pragma endregion


#pragma region OTNStreamWriter

	/**
	* @brief Streaming writer for large OTN files — writes objects and rows one at a time.
	*
	* Two complementary workflows:
	*
	* BeginObject / WriteRow / EndObject — for raw streaming of large data.
	*
	* Values of custom types (ToOTNDataType) or OTNObjects are written as rows of their own
	* object and referenced by index. Integer indices can be written directly as well,
	* use GetWrittenRowCount() to determine the next available index.
	*
	* AppendObject:
	*
	* for OTNObject instances that may contain nested custom objects.
	* All referenced sub-objects are written as well.
	*
	* notes:
	*
	* - All rows are collected in a temporary file ("<name>" + TEMP_FILE_EXTENSION next to the target file).
	*   The target file is written on Close(), every object becomes one block with its final row count.
	*
	* - The same object name can be used multiple times (BeginObject, AppendObject, sub-objects),
	*   the rows are appended to the same block. Column names and types have to match.
	*
	* - The same sub-object instance is written only once, copies of values are written as new rows.
	*
	* - DefType/DefName optimizations are not applied (full type names are written).
	*
	* - BeginObject/WriteRow and AppendObject may be freely intermixed.
	*
	* - Object, column and type names and the values of every row are checked against the columns,
	*   on an error the writer becomes invalid (see GetError()).
	*
	* - Files are fully compatible with OTNReader and OTNStreamReader.
	*/
	class OTNStreamWriter {
	public:
		OTNStreamWriter() = default;
		~OTNStreamWriter();

		/**
		* @brief Open a file for streaming writes.
		* @param path File path (extension .otn added automatically if missing).
		* @return True on success.
		*/
		bool Open(const OTNFilePath& path);

		/**
		* @brief Begin writing an object — schema given as two string vectors.
		*
		* Column types follow the same syntax as OTNObject::SetTypes():
		*
		* "int", "float[]", "String", "MyObjectName", "MyObjectName[]", etc.
		*
		* For columns that reference another OTN object use the object's name as the
		* type string (e.g. "WeaponStats") and write the values (custom types, OTNObjects)
		* or integer row indices in WriteRow.
		*
		* If the object name was already used, the rows are appended to it. The schema has to match.
		*
		* @return True on success.
		*/
		bool BeginObject(
			const std::string& name,
			const std::vector<std::string>& columnNames,
			const std::vector<std::string>& columnTypes);

		/**
		* @brief Begin writing an object — schema given as variadic OTNColumnDef arguments.
		*
		* @code
		* w.BeginObject("Weapon",
		*     OTN::col("id",    "int"),
		*     OTN::col("name",  "String"),
		*     OTN::col("stats", "WeaponStats"));
		* @endcode
		*
		* @tparam Cols  Each must be convertible to OTNColumnDef.
		* @return True on success.
		*/
		template<typename... Cols,
			std::enable_if_t<(std::is_convertible_v<std::decay_t<Cols>, OTNColumnDef> && ...), int> = 0>
		bool BeginObject(const std::string& name, Cols&&... cols) {
			std::vector<std::string> names, types;

			names.reserve(sizeof...(Cols));
			types.reserve(sizeof...(Cols));

			([&](OTNColumnDef cd) {
				names.push_back(std::move(cd.name));
				types.push_back(std::move(cd.type));
			}(static_cast<OTNColumnDef>(std::forward<Cols>(cols))), ...);

			return BeginObject(name, names, types);
		}

		/**
		* @brief Write one data row using variadic arguments.
		*
		* Values must match the column schema set in BeginObject.
		* For columns referencing other objects, pass the value (custom type, OTNObject)
		* or the integer row index.
		*
		* @return True on success.
		*/
		template<typename... Args>
		bool WriteRow(Args&&... args) {
			constexpr size_t N = sizeof...(Args);
			if (!CheckRowPreconditions(N))
				return false;

			OTNRow row;
			row.reserve(N);
			bool rowValid = true;

			auto addValue = [&](auto&& v) {
				if (!rowValid)
					return;

				using DT = std::decay_t<decltype(v)>;
				Detail::OTNDataType<DT> dt(std::forward<decltype(v)>(v));
				if (!dt.m_valid) {
					AddError(dt.m_error);
					rowValid = false;
					return;
				}

				row.emplace_back(std::move(dt.m_value));
			};

			(addValue(std::forward<Args>(args)), ...);

			if (!rowValid)
				return false;
			return WriteRowInternal(row);
		}

		/**
		* @brief Write one data row from a pre-built OTNRow.
		* @return True on success.
		*/
		bool WriteRowList(const OTNRow& row);

		/**
		* @brief Finish the current object. The rows stay in the temporary file until Close().
		* @return True on success.
		*/
		bool EndObject();

		// ---- High-level append ---------------------------------------------------

		/**
		* @brief Serialize a complete OTNObject (with all nested objects) in one call.
		*
		* Nested objects (values of type OBJECT / custom types serialised via
		* ToOTNDataType) are written as rows of their own object. Repeated identical
		* object pointers are deduplicated: the same sub-object instance is written
		* only once, and all references to it share the same row index.
		*
		* @code
		* // Custom type is serialized automatically
		* OTNObject layer("Layer");
		* layer.SetNames("index","cfg").AddDataRow(0, myLayerConfig);
		* writer.AppendObject(layer);
		* @endcode
		*
		* @return True on success.
		*/
		bool AppendObject(const OTNObject& obj);

		// ---- Index query ---------------------------------------------------------

		/**
		* @brief Returns how many rows have been written so far for the given object name.
		*
		* Use this to build integer indices for parent objects that reference sub-objects
		* written earlier:
		*
		* @code
		* size_t idx = w.GetWrittenRowCount("WeaponStats"); // next free index
		* w.BeginObject("WeaponStats",...);
		* w.WriteRow(10.5f, 1.0f);
		* w.EndObject();
		* // now pass idx as the "stats" column value in the parent object
		* @endcode
		*/
		size_t GetWrittenRowCount(const std::string& objectName) const;

		// ---- Lifecycle -----------------------------------------------------------

		/**
		* @brief Writes all objects into the file and closes it. Calls EndObject() if an object is open.
		* @return True on success.
		*/
		bool Close();

		bool IsValid() const;
		std::string GetError() const;
		bool TryGetError(std::string& outError) const;

	private:
		static constexpr size_t COPY_BUFFER_SIZE = 64 * 1024; // 64 KB
		static constexpr size_t NO_BLOCK = static_cast<size_t>(-1);

		/// part of the temporary file that contains rows of one object
		struct RowRange {
			uint64_t offset = 0;
			uint64_t length = 0;
		};

		/// one object of the target file, its rows can be spread over the temporary file
		struct ObjectBlock {
			Detail::ObjectHeaderInfo header; // rowCount is the number of rows written so far
			std::vector<RowRange> ranges;
		};

		struct WrittenObject {
			std::weak_ptr<OTNObject> owner; // used to detect if the address was reused by a new object
			size_t index = 0;
		};

		Detail::WriteOptions m_writeOptions;
		Detail::FileHeaderInfo m_fileHeader;
		std::ofstream m_file;
		OTNFilePath m_filePath;
		bool m_valid = true;
		bool m_fileOpen = false;
		bool m_objectOpen = false;

		std::ofstream m_rowFile;      // temporary file for the rows of all objects
		OTNFilePath m_rowFilePath;
		uint64_t m_rowFileSize = 0;

		std::vector<ObjectBlock> m_blocks; // in order of the first use
		std::unordered_map<std::string, size_t> m_blockIndex;
		size_t m_currentBlock = NO_BLOCK; // object open via BeginObject
		std::string m_error;

		// Maps raw OTNObject pointer of sub-objects → first row index in its block, for deduplication.
		std::unordered_map<const OTNObject*, WrittenObject> m_objPtrToIndex;
		// sub-objects whose rows are currently converted, used to detect cyclic objects
		std::unordered_set<const OTNObject*> m_objectsInProgress;

		/**
		* @brief Returns the first row index of an already written sub-object.
		*/
		std::optional<size_t> FindWrittenIndex(const Detail::OTNObjectPtr& ptr) const;

		/**
		* @brief Appends the row indices of a written sub-object to outIndices.
		*
		* Objects build from T are referenced by one index, native objects by one index per row.
		*/
		static void AppendObjectIndices(const Detail::OTNObjectPtr& ptr, size_t first, std::vector<size_t>& outIndices);

		/**
		* @brief Writes a sub-object (if not written yet) and returns its row indices.
		*/
		bool WriteSubObject(const Detail::OTNObjectPtr& ptr, std::vector<size_t>& outIndices);

		/**
		* @brief Writes all rows of an object into its block.
		*
		* Sub-objects are written first, so the rows of the object stay together.
		*/
		bool WriteObjectRows(const OTNObject& obj, size_t& outFirstRow);

		/**
		* @brief Convert a single OTNValue for stream output, sub-objects are written.
		*
		* OTNObjectPtr   → integer index, or [idx, ...] if colType is a list
		* LIST of OBJECT → [idx, ...]     (list of integer refs)
		* Everything else → unchanged.
		*/
		bool ConvertValue(const OTNValue& val, const OTNTypeDesc& colType, OTNValue& outValue);
		bool ConvertRow(const OTNRow& row, const std::vector<OTNTypeDesc>& colTypes, OTNRow& outRow);

		/**
		* @brief Returns the block of the object, creates it if the name is new.
		*
		* Fails if the name is already used with other column names or types.
		*/
		bool GetBlock(const std::string& name, const std::vector<std::string>& colNames,
			const std::vector<OTNTypeDesc>& colTypes, size_t& outBlock);

		// appends written rows to the temporary file and the block
		bool AppendRows(size_t block, const std::string& rows, size_t rowCount);

		// writes all blocks into the target file
		bool WriteFile();
		bool CopyRanges(const ObjectBlock& block, std::ifstream& in);

		void ResetFileState();
		bool CheckRowPreconditions(size_t argCount);
		bool WriteRowInternal(const OTNRow& row);
		// closes and deletes the temporary row file
		void DiscardRowFile();
		void AddError(const std::string& error);
	};

#pragma endregion

#pragma region OTNReader

	/**
	* @brief Reads OTN files and provides access to stored OTNObjects.
	*
	* Supports file validation, version checking, and error reporting.
	*/
	class OTNReader {
	public:
		/**
		* @brief Default constructor.
		*/
		explicit OTNReader() = default;

		/**
		* @brief Destructor.
		*/
		~OTNReader() = default;

		/**
		* @brief Read an OTN file from the specified path.
		*
		* Validates the path, opens the file, and reads its data.
		*
		* @param path Absolute path including file name (e.g., "file.otn" or "file").
		* @return True if saving succeeded, false otherwise. Retrieve more information via GetError() or TryGetError()
		*/
		bool ReadFile(const OTNFilePath& path);

		/**
		* @brief Read OTN data directly from a string buffer.
		*
		* Useful for tests, network payloads, or already-loaded text content.
		*
		* @param fileString OTN text data.
		* @return True if parsing succeeded, false otherwise.
		*/
		bool ReadString(const std::string& fileString);

		/**
		* @brief Returns the version of the OTN file.
		* @return Version number; values <= 0 indicate an invalid version.
		*/
		uint8_t GetVersion() const;

		/**
		* @brief Retrieves a stored OTNObject by name.
		* @param objName Name of the object to retrieve.
		* @return Optional containing the object if found, std::nullopt otherwise.
		*/
		std::optional<OTNObject> TryGetObject(const std::string& objName) const;

		/**
		* @brief Returns a stored OTNObject by name without copying it.
		* @param objName Name of the object.
		* @return Pointer to the object, nullptr if not found.
		*
		* The pointer stays valid until the next ReadFile / ReadString call or until the reader is destroyed.
		*/
		const OTNObject* FindObject(const std::string& objName) const;
		OTNObject* FindObject(const std::string& objName);

		/**
		* @brief Get all loaded OTNObjects (read-only).
		*
		* Use this to inspect objects without modifying them.
		*
		* @return Const reference to the internal map of object names to OTNObjects.
		*/
		const std::unordered_map<std::string, OTNObject>& GetObjects() const;

		/**
		* @brief Get all loaded OTNObjects (modifiable).
		*
		* Use this to modify or insert objects after reading.
		*
		* @return Reference to the internal map of object names to OTNObjects.
		*/
		std::unordered_map<std::string, OTNObject>& GetObjects();

		/**
		* @brief Returns true if the reader is in a valid state (no errors).
		*/
		bool IsValid() const;

		/**
		* @brief Returns the current error message string.
		*/
		std::string GetError() const;

		/**
		* @brief Attempts to get the current error message.
		* @param outError Reference to store the error string.
		* @return True if an error exists, false if the reader is valid.
		*/
		bool TryGetError(std::string& outError) const;

	private:
		class ReaderData {
		public:
			uint8_t version = 0;
			std::unordered_map<std::string, OTNObject> objects;

			void Reset() {
				version = 0;
				objects.clear();
			}
		};

		// object rows that are currently resolved, used to detect cyclic references
		using RefStack = std::vector<std::pair<std::string, size_t>>;

		std::string m_error;
		bool m_valid = true;
		ReaderData m_readerData;

		bool ReadData(std::string_view src, ReaderData& data);

		// replaces the Ref<X> values of all objects by the referenced rows
		bool ResolveObjectRefs(ReaderData& data);
		bool ResolveValue(ReaderData& data, const std::string& ownerObjectName, OTNValue& value, RefStack& stack);

		void AddError(const std::string& error);
	};

#pragma endregion


#pragma region OTNStreamReader

	/**
	* @brief Streaming reader for large OTN files — reads objects and rows one at a time.
	*
	* Unlike OTNReader, the whole file is never loaded into RAM.
	* On Open() the file is scanned once to build a small index (objects, columns,
	* row counts and the file position of every ROW_CHECKPOINT_INTERVAL-th row).
	* Navigate sequentially with NextObject() / ReadRow(), or read any object or row
	* directly with ReadObject() / ReadObjectRow().
	*
	* Usage:
	* @code
	* OTN::OTNStreamReader r;
	* if (r.Open("model.otn")) {
	*     while (r.NextObject()) {
	*         std::cout << r.GetCurrentObjectName() << "\n";
	*         OTN::OTNRow row;
	*         while (r.ReadRow(row)) {
	*             // process row …
	*         }
	*     }
	* }
	* @endcode
	*
	* notes:
	*
	* - Calling NextObject() without reading all rows auto-skips remaining rows.
	*
	* - Object references are resolved to OTNObjects (one row each) by default,
	*   see SetResolveObjectRefs(). Unresolved references are Detail::OTNObjectRef values.
	*
	* - Before every read operation the file size and modification time are checked.
	*   If the file was changed since the scan, it is scanned again automatically and
	*   the current read position (object name + row) is restored.
	*/
	class OTNStreamReader {
	public:
		OTNStreamReader();
		~OTNStreamReader();

		/**
		* @brief Open an OTN file for streaming reads and scan its objects.
		* @param path File path (extension .otn added automatically if missing).
		* @return True on success.
		*/
		bool Open(const OTNFilePath& path);

		/**
		* @brief Advance to the next object in the file.
		* @return True if an object was found, false at end-of-file or on error.
		*/
		bool NextObject();

		/**
		* @brief Returns true if there is at least one more unread object in the stream.
		*/
		bool HasMoreObjects() const;

		/** @return Name of the currently loaded object (valid after NextObject()). */
		const std::string& GetCurrentObjectName() const;

		/** @return Column names for the current object. */
		const std::vector<std::string>& GetColumnNames() const;

		/** @return Column type descriptors for the current object. */
		const std::vector<OTNTypeDesc>& GetColumnTypes() const;

		/** @return Number of rows still to be read in the current object. */
		size_t GetRemainingRowCount() const;

		/**
		* @brief Read the next row of the current object.
		* After the last row use NextObject or HasMoreObjects
		*
		* @param outRow Output row.
		* @return True if a row was read, false when all rows are consumed.
		*/
		bool ReadRow(OTNRow& outRow);

		/**
		* @brief Returns true while there are unread rows in the current object.
		*/
		bool HasMoreRows() const;

		/**
		* @brief Skip all remaining rows of the current object.
		* After this call, NextObject() may be called immediately.
		* @return True on success.
		*/
		bool SkipCurrentObject();

		/**
		* @brief Returns the names of all objects in the file (in file order).
		*/
		std::vector<std::string> GetObjectNames();

		/**
		* @brief Read a single row of any object, independent of the sequential read position.
		* @param objectName Name of the object.
		* @param rowIndex Row index (0-based).
		* @return OTNObject with exactly this row, std::nullopt if the object or row does not exist or on error.
		*/
		std::optional<OTNObject> ReadObjectRow(const std::string& objectName, size_t rowIndex);

		/**
		* @brief Read a complete object with all rows into RAM.
		*
		* note:
		*
		* - Only meant for small objects, use NextObject() / ReadRow() for large ones.
		*
		* @param objectName Name of the object.
		* @return The object, std::nullopt if it does not exist or on error.
		*/
		std::optional<OTNObject> ReadObject(const std::string& objectName);

		/**
		* @brief Enable or disable resolving object references (default: enabled).
		*
		* If enabled, values of object columns are loaded as OTNObject (recursively),
		* so they can be read with GetValue<MyType>(). If disabled they stay Detail::OTNObjectRef.
		*/
		void SetResolveObjectRefs(bool value);

		/** @return True if object references are resolved. */
		bool GetResolveObjectRefs() const;

		/** @return OTN format version (available after Open()). */
		uint8_t GetVersion() const;

		bool IsValid() const;
		const std::string& GetError() const;
		bool TryGetError(std::string& outError) const;

	private:
		// every n-th row the file position is stored in the index
		static constexpr size_t ROW_CHECKPOINT_INTERVAL = 1024;
		static constexpr size_t NO_OBJECT = static_cast<size_t>(-1);

		// index entry of one object, created by Scan()
		struct ObjectInfo {
			Detail::ObjectHeaderInfo header;
			// position of row 0, ROW_CHECKPOINT_INTERVAL, 2 * ROW_CHECKPOINT_INTERVAL, ...
			std::vector<Detail::FilePosition> rowCheckpoints;
		};

		// object row currently being resolved, used to detect cyclic references
		struct RowKey {
			std::string objectName;
			size_t row = 0;
		};

		std::unique_ptr<Detail::CharSource> m_source;
		std::unique_ptr<Detail::Tokenizer> m_tok;
		const Detail::ReadSyntax* m_syntax = nullptr;
		Detail::FileHeaderInfo m_fileHeader;

		OTNFilePath m_path;
		uintmax_t m_fileSize = 0;
		std::filesystem::file_time_type m_fileTime{};

		std::vector<ObjectInfo> m_objects;
		std::unordered_map<std::string, size_t> m_objectIndex;

		bool m_valid = true;
		bool m_resolveObjectRefs = true;
		std::string m_error;

		// sequential read position
		size_t m_currentObject = NO_OBJECT;
		size_t m_currentRow = 0;
		std::string m_currentName;
		std::vector<std::string> m_columnNames;
		std::vector<OTNTypeDesc> m_columnTypes;
		size_t m_remainingRows = 0;

		// ---- index ----
		bool Scan();
		bool EnsureUpToDate();
		bool ReadFileStamp(uintmax_t& outSize, std::filesystem::file_time_type& outTime) const;
		const ObjectInfo* FindObject(const std::string& name) const;
		void SetCurrentObject(size_t objectIndex, size_t row);
		bool SeekToRow(const ObjectInfo& info, size_t row);
		bool ReadRowValues(const ObjectInfo& info, OTNRow& outRow);

		// ---- object references ----
		std::optional<OTNObject> LoadObjectRow(const ObjectInfo& info, size_t row, std::vector<RowKey>& stack);
		bool ResolveRow(OTNRow& row, std::vector<RowKey>& stack);
		bool ResolveValue(OTNValue& value, std::vector<RowKey>& stack);

		// adds the error of the tokenizer, always returns false
		bool TokenizerFailed();
		void AddError(const std::string& msg);
	};

#pragma endregion

}