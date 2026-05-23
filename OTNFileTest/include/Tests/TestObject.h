#pragma once
#include <cmath>
#include <vector>
#include <string>
#include <filesystem>
#include <OTNFile/OTNFile.h>
#include "TestFramework.h"
#include "TestHelpers.h"

// ============================================================
//  Construction & basic accessors
// ============================================================

inline void Test_Object_BasicConstruction(const std::filesystem::path&) {
    OTN::OTNObject obj("TestObj");

    CHECK_EQ(obj.GetObjectName(), std::string("TestObj"));
    CHECK(obj.IsValid());
    CHECK_EQ(obj.GetRowCount(), size_t(0));
    CHECK_EQ(obj.GetColumnCount(), size_t(0));
}

// ============================================================
//  All primitive types in one row
// ============================================================

inline void Test_Object_AllPrimitiveTypes(const std::filesystem::path&) {
    OTN::OTNObject obj("Primitives");
    obj.SetNames("i", "i64", "u64", "f", "d", "b", "s")
       .AddDataRow(42,
                   int64_t(-9000000000LL),
                   uint64_t(18000000000ULL),
                   3.14f,
                   2.718281828,
                   true,
                   std::string("hello"));

    CHECK(obj.IsValid());
    CHECK_EQ(obj.GetRowCount(), size_t(1));
    CHECK_EQ(obj.GetValue<int>(0, "i"), 42);
    CHECK_EQ(obj.GetValue<int64_t>(0, "i64"), int64_t(-9000000000LL));
    CHECK_EQ(obj.GetValue<uint64_t>(0, "u64"), uint64_t(18000000000ULL));
    CHECK(std::abs(obj.GetValue<float>(0,  "f") - 3.14f) < 1e-4f);
    CHECK(std::abs(obj.GetValue<double>(0, "d") - 2.718281828)< 1e-9);
    CHECK_EQ(obj.GetValue<bool>(0, "b"), true);
    CHECK_EQ(obj.GetValue<std::string>(0, "s"), std::string("hello"));
}

// ============================================================
//  Multiple rows (100 rows, verify first & last)
// ============================================================

inline void Test_Object_MultipleRows(const std::filesystem::path&) {
    OTN::OTNObject obj("Multi");
    obj.SetNames("id", "val");
    for (int i = 0; i < 100; ++i)
        obj.AddDataRow(i, i * 2.5f);

    CHECK(obj.IsValid());
    CHECK_EQ(obj.GetRowCount(), size_t(100));
    CHECK_EQ(obj.GetValue<int>(0, "id"), 0);
    CHECK_EQ(obj.GetValue<int>(99, "id"), 99);
    CHECK(std::abs(obj.GetValue<float>(50, "val") - 50 * 2.5f) < 1e-4f);
}

// ============================================================
//  TryGetValue: valid access, bad row, bad column
// ============================================================

inline void Test_Object_TryGetValue(const std::filesystem::path&) {
    OTN::OTNObject obj("TryGet");
    obj.SetNames("x").AddDataRow(7);

    auto v = obj.TryGetValue<int>(0, "x");
    CHECK(v.has_value());
    CHECK_EQ(*v, 7);

    CHECK(!obj.TryGetValue<int>(99, "x").has_value());          // bad row
    CHECK(!obj.TryGetValue<int>(0, "nonexistent").has_value()); // bad column
}

// ============================================================
//  GetValue with default fallback
// ============================================================

inline void Test_Object_GetValueDefault(const std::filesystem::path&) {
    OTN::OTNObject obj("Defaults");
    obj.SetNames("n").AddDataRow(10);

    CHECK_EQ(obj.GetValue<int>(0, "n", -1), 10);        // found
    CHECK_EQ(obj.GetValue<int>(0, "missing", -1), -1);  // bad column
    CHECK_EQ(obj.GetValue<int>(99, "n", -1), -1);       // bad row
}

// ============================================================
//  Column access by index
// ============================================================

inline void Test_Object_ColumnByIndex(const std::filesystem::path&) {
    OTN::OTNObject obj("ByIdx");
    obj.SetNames("a", "b", "c").AddDataRow(1, 2, 3);

    CHECK_EQ(obj.GetValue<int>(0, size_t(0)), 1);
    CHECK_EQ(obj.GetValue<int>(0, size_t(1)), 2);
    CHECK_EQ(obj.GetValue<int>(0, size_t(2)), 3);
}

// ============================================================
//  Vector column (non-empty)
// ============================================================

inline void Test_Object_VectorColumn(const std::filesystem::path&) {
    std::vector<int> data = {10, 20, 30};
    OTN::OTNObject obj("Lists");
    obj.SetNames("nums").AddDataRow(data);

    CHECK(obj.IsValid());
    auto got = obj.TryGetValue<std::vector<int>>(0, "nums");
    CHECK(got.has_value());
    if (got) {
        CHECK_EQ(got->size(), size_t(3));
        CHECK_EQ((*got)[0], 10);
        CHECK_EQ((*got)[2], 30);
    }
}

// ============================================================
//  Empty vector column
// ============================================================

inline void Test_Object_EmptyVector(const std::filesystem::path&) {
    std::vector<float> empty;
    OTN::OTNObject obj("EmptyList");
    obj.SetNames("data").AddDataRow(empty);

    CHECK(obj.IsValid());
    auto got = obj.TryGetValue<std::vector<float>>(0, "data");
    CHECK(got.has_value());
    if (got) 
        CHECK_EQ(got->size(), size_t(0));
}

// ============================================================
//  SetTypes explicit before AddDataRow
// ============================================================

inline void Test_Object_ExplicitSetTypes(const std::filesystem::path&) {
    OTN::OTNObject obj("Typed");
    obj.SetNames("val").SetTypes("float").AddDataRow(1.5f);

    CHECK(obj.IsValid());
    CHECK(std::abs(obj.GetValue<float>(0, "val") - 1.5f) < 1e-5f);
}

// ============================================================
//  String with special characters
// ============================================================

inline void Test_Object_SpecialStrings(const std::filesystem::path&) {
    OTN::OTNObject obj("SpecStr");
    std::string special = "Hello, World! \t 123 @#$%";
    obj.SetNames("s").AddDataRow(special);

    CHECK(obj.IsValid());
    CHECK_EQ(obj.GetValue<std::string>(0, "s"), special);
}

// ============================================================
//  Boolean values (true / false)
// ============================================================

inline void Test_Object_Booleans(const std::filesystem::path&) {
    OTN::OTNObject obj("Bools");
    obj.SetNames("t", "f").AddDataRow(true, false);

    CHECK(obj.IsValid());
    CHECK_EQ(obj.GetValue<bool>(0, "t"), true);
    CHECK_EQ(obj.GetValue<bool>(0, "f"), false);
}

// ============================================================
//  Numeric edge cases (0, INT_MIN, INT_MAX)
// ============================================================

inline void Test_Object_NumericEdgeCases(const std::filesystem::path&) {
    const int max = INT32_MAX;
    const int min = INT32_MIN;

    OTN::OTNObject obj("NumEdge");
    obj.SetNames("zero", "neg", "maxInt")
       .AddDataRow(0,  min, max);

    CHECK(obj.IsValid());
    CHECK_EQ(obj.GetValue<int>(0, "zero"), 0);
    CHECK_EQ(obj.GetValue<int>(0, "neg"), min);
    CHECK_EQ(obj.GetValue<int>(0, "maxInt"), max);
}

// ============================================================
//  SetObjectName
// ============================================================

inline void Test_Object_SetObjectName(const std::filesystem::path&) {
    OTN::OTNObject obj("OldName");

    CHECK(obj.SetObjectName("NewName"));
    CHECK_EQ(obj.GetObjectName(), std::string("NewName"));
}

// ============================================================
//  ReserveDataRows (correctness unaffected)
// ============================================================

inline void Test_Object_Reserve(const std::filesystem::path&) {
    OTN::OTNObject obj("Reserved");
    obj.SetNames("n").ReserveDataRows(50);
    for (int i = 0; i < 10; ++i)
        obj.AddDataRow(i);

    CHECK(obj.IsValid());
    CHECK_EQ(obj.GetRowCount(), size_t(10));
}

// ============================================================
//  Custom type (Vec2) stored as a column value
// ============================================================

inline void Test_Object_CustomType(const std::filesystem::path&) {
    Vec2 v{1.0f, 2.0f};
    OTN::OTNObject obj("Vecs");
    obj.SetNames("pos").AddDataRow(v);

    CHECK(obj.IsValid());
    auto got = obj.TryGetValue<Vec2>(0, "pos");
    CHECK(got.has_value());
    if (got) {
        CHECK(std::abs(got->x - 1.0f) < 1e-5f);
        CHECK(std::abs(got->y - 2.0f) < 1e-5f);
    }
}

// ============================================================
//  SetNamesList / SetTypesList (vector overloads)
// ============================================================

inline void Test_Object_VectorSetNamesAndTypes(const std::filesystem::path&) {
    OTN::OTNObject obj("VecInit");
    obj.SetNamesList({"a", "b", "c"})
       .SetTypesList({"int", "float", "String"})
       .AddDataRow(7, 1.5f, std::string("test"));

    CHECK(obj.IsValid());
    CHECK_EQ(obj.GetColumnCount(), size_t(3));
    CHECK_EQ(obj.GetValue<int>(0, "a"), 7);
    CHECK_EQ(obj.GetValue<std::string>(0, "c"), std::string("test"));
}

// ============================================================
//  AddDataRowList
// ============================================================

inline void Test_Object_AddDataRowList(const std::filesystem::path&) {
    OTN::OTNObject obj("RowList");
    obj.SetNames("x", "y");

    OTN::OTNRow row;
    row.emplace_back(OTN::OTNValueVariant(int(3)));
    row.emplace_back(OTN::OTNValueVariant(int(4)));
    obj.AddDataRowList(row);

    CHECK(obj.IsValid());
    CHECK_EQ(obj.GetRowCount(), size_t(1));
    CHECK_EQ(obj.GetValue<int>(0, "x"), 3);
    CHECK_EQ(obj.GetValue<int>(0, "y"), 4);
}

// ============================================================
//  Entry point — call all object tests
// ============================================================

inline void RunObjectTests(const std::filesystem::path& basePath) {
    std::cout << "\n--- OTNObject Tests ---\n";
    RUN(Test_Object_BasicConstruction);
    RUN(Test_Object_AllPrimitiveTypes);
    RUN(Test_Object_MultipleRows);
    RUN(Test_Object_TryGetValue);
    RUN(Test_Object_GetValueDefault);
    RUN(Test_Object_ColumnByIndex);
    RUN(Test_Object_VectorColumn);
    RUN(Test_Object_EmptyVector);
    RUN(Test_Object_ExplicitSetTypes);
    RUN(Test_Object_SpecialStrings);
    RUN(Test_Object_Booleans);
    RUN(Test_Object_NumericEdgeCases);
    RUN(Test_Object_SetObjectName);
    RUN(Test_Object_Reserve);
    RUN(Test_Object_CustomType);
    RUN(Test_Object_VectorSetNamesAndTypes);
    RUN(Test_Object_AddDataRowList);
}
