#pragma once
#include <string>
#include <optional>
#include <fstream>
#include <filesystem>
#include <OTNFile/OTNFile.h>
#include "TestFramework.h"
#include "TestHelpers.h"

// ============================================================
//  Custom types used by the regression tests
// ============================================================

// serializes data without names -> builder becomes invalid
struct BrokenType {
    int v = 0;
};

template<>
inline void OTN::ToOTNDataType<BrokenType>(OTN::OTNObjectBuilder& b, BrokenType& x) {
    b.SetObjectName("BrokenType");
    b.AddData(x.v);
}

struct NameDataType {
    int v = 0;
};

template<>
inline void OTN::ToOTNDataType<NameDataType>(OTN::OTNObjectBuilder& b, NameDataType& x) {
    b.SetObjectName("NameDataType");
    b.AddNameData("v", x.v);
    b.AddNameData("constant", 5); // rvalue
}

struct UIntType {
    uint32_t v = 0;
};

template<>
inline void OTN::ToOTNDataType<UIntType>(OTN::OTNObjectBuilder& b, UIntType& x) {
    b.SetObjectName("UIntType");
    b.AddNames("v");
    b.AddData(x.v);
}

struct ObjectMemberType {
    OTN::OTNObject inner{ "Inner" };
};

template<>
inline void OTN::ToOTNDataType<ObjectMemberType>(OTN::OTNObjectBuilder& b, ObjectMemberType& x) {
    b.SetObjectName("ObjectMemberType");
    b.AddNames("inner");
    b.AddData(x.inner);
}

// ============================================================
//  Strings with quotes, backslashes, newlines and tabs
// ============================================================

inline void Test_Bugfix_StringEscape(const std::filesystem::path& dir) {
    const std::string value = "a\"b\\c\nd\te";

    OTN::OTNObject obj("Esc");
    obj.SetNames("s").AddDataRow(value);

    OTN::OTNWriter w;
    w.AppendObject(obj);
    std::string text;
    CHECK(w.SaveToString(text));

    OTN::OTNReader r;
    CHECK(r.ReadString(text));
    auto read = r.TryGetObject("Esc");
    CHECK(read.has_value());
    if (read)
        CHECK_EQ(read->GetValue<std::string>(0, "s", ""), value);

    // stream writer
    auto path = MakeTestFile(dir, "bugfix_string_escape");
    OTN::OTNStreamWriter sw;
    CHECK(sw.Open(path));
    CHECK(sw.AppendObject(obj));
    CHECK(sw.Close());

    OTN::OTNReader r2;
    CHECK(r2.ReadFile(path));
    auto read2 = r2.TryGetObject("Esc");
    CHECK(read2.has_value());
    if (read2)
        CHECK_EQ(read2->GetValue<std::string>(0, "s", ""), value);
}

// ============================================================
//  defType with list columns keeps the list depth
// ============================================================

inline void Test_Bugfix_DefTypeList(const std::filesystem::path&) {
    OTN::OTNObject obj("DefList");
    obj.SetNames("a", "b", "c", "d")
        .AddDataRow(std::vector<int>{ 1, 2 }, std::vector<int>{ 3 }, 4, 5);

    OTN::OTNWriter w;
    w.UseDefType(true).AppendObject(obj);
    std::string text;
    CHECK(w.SaveToString(text));

    OTN::OTNReader r;
    CHECK(r.ReadString(text));
    auto read = r.TryGetObject("DefList");
    CHECK(read.has_value());
    if (!read)
        return;

    auto a = read->TryGetValue<std::vector<int>>(0, "a");
    CHECK(a.has_value());
    if (a)
        CHECK_EQ(a->size(), size_t(2));
    CHECK_EQ(read->GetValue<int>(0, "d", 0), 5);
}

// ============================================================
//  StreamWriter: a new object at the address of an old one is written
// ============================================================

inline void Test_Bugfix_StreamAddressReuse(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "bugfix_stream_address_reuse");

    OTN::OTNStreamWriter sw;
    CHECK(sw.Open(path));

    std::optional<OTN::OTNObject> obj; // emplace reuses the same address
    obj.emplace("First");
    obj->SetNames("x").AddDataRow(1);
    CHECK(sw.AppendObject(*obj));

    obj.emplace("Second");
    obj->SetNames("y").AddDataRow(2);
    CHECK(sw.AppendObject(*obj));
    CHECK(sw.Close());

    OTN::OTNReader r;
    CHECK(r.ReadFile(path));
    CHECK(r.TryGetObject("First").has_value());
    CHECK(r.TryGetObject("Second").has_value());
}

// ============================================================
//  StreamWriter: shared sub-object over multiple AppendObject calls
// ============================================================

inline void Test_Bugfix_StreamSharedSubObject(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "bugfix_stream_shared_sub");

    OTN::OTNObject first("FirstParent");
    first.SetNames("pos").AddDataRow(Vec2{ 1.0f, 2.0f });
    const OTN::OTNValue shared = first.GetRow(0)[0];

    OTN::OTNObject second("SecondParent");
    second.SetNames("pos").AddDataRowList({ shared });

    OTN::OTNStreamWriter sw;
    CHECK(sw.Open(path));
    CHECK(sw.AppendObject(first));
    CHECK(sw.AppendObject(second)); // Vec2 already written -> only referenced
    CHECK(sw.Close());

    OTN::OTNReader r;
    CHECK(r.ReadFile(path));
    auto read = r.TryGetObject("SecondParent");
    CHECK(read.has_value());
    if (!read)
        return;

    auto pos = read->TryGetValue<Vec2>(0, "pos");
    CHECK(pos.has_value());
    if (pos)
        CHECK_EQ(pos->y, 2.0f);
}

// ============================================================
//  AddDataRow with an invalid first row
// ============================================================

inline void Test_Bugfix_AddDataRowInvalidFirstRow(const std::filesystem::path&) {
    OTN::OTNObject obj("InvalidFirst");
    obj.SetNames("b").AddDataRow(BrokenType{});
    CHECK(!obj.IsValid());
    CHECK_EQ(obj.GetRowCount(), size_t(0));
}

// ============================================================
//  StreamWriter: object without rows and types
// ============================================================

inline void Test_Bugfix_StreamObjectWithoutTypes(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "bugfix_stream_no_types");

    OTN::OTNObject obj("NoTypes");
    obj.SetNames("a", "b");

    OTN::OTNStreamWriter sw;
    CHECK(sw.Open(path));
    CHECK(!sw.AppendObject(obj));
    CHECK(!sw.IsValid());
    sw.Close();

    // with explicit types an empty object can be written
    OTN::OTNObject typed("Typed");
    typed.SetNames("a", "b").SetTypes("int", "float");

    OTN::OTNStreamWriter sw2;
    CHECK(sw2.Open(path));
    CHECK(sw2.AppendObject(typed));
    CHECK(sw2.Close());
}

// ============================================================
//  StreamWriter: nested native object with multiple rows
// ============================================================

inline void Test_Bugfix_StreamMultiRowNested(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "bugfix_stream_multirow");

    OTN::OTNObject child("Child");
    child.SetNames("v").AddDataRow(1).AddDataRow(2).AddDataRow(3);

    OTN::OTNObject parent("Parent");
    parent.SetNames("c").AddDataRow(child);

    OTN::OTNStreamWriter sw;
    CHECK(sw.Open(path));
    CHECK(sw.AppendObject(parent));
    CHECK(sw.Close());

    OTN::OTNReader r;
    CHECK(r.ReadFile(path));
    auto read = r.TryGetObject("Parent");
    CHECK(read.has_value());
    if (!read)
        return;

    auto children = read->TryGetValue<std::vector<OTN::OTNObject>>(0, "c");
    CHECK(children.has_value());
    if (!children)
        return;

    CHECK_EQ(children->size(), size_t(3));
    if (children->size() == 3)
        CHECK_EQ((*children)[2].GetValue<int>(0, "v", 0), 3);
}

// ============================================================
//  Copy assignment resets the column cache
// ============================================================

inline void Test_Bugfix_CopyAssignmentCache(const std::filesystem::path&) {
    OTN::OTNObject a("A");
    a.SetNames("x", "y").AddDataRow(1, 2);
    OTN::OTNObject b("B");
    b.SetNames("y", "x").AddDataRow(10, 20);

    CHECK_EQ(a.GetValue<int>(0, "y"), 2); // fills the cache
    a = b;
    CHECK_EQ(a.GetValue<int>(0, "y"), 10);
}

// ============================================================
//  OTNWriter: options and objects are handled per save
// ============================================================

inline void Test_Bugfix_WriterMultipleSaves(const std::filesystem::path&) {
    OTN::OTNObject o1("N");
    o1.SetNames("position", "velocity").AddDataRow(1, 2);
    OTN::OTNObject o2("M");
    o2.SetNames("position", "velocity").AddDataRow(3, 4);

    OTN::OTNWriter w;
    w.AppendObject(o1).AppendObject(o2);

    std::string withDef;
    std::string withoutDef;
    CHECK(w.UseDefName(true).SaveToString(withDef));
    CHECK(w.UseDefName(false).SaveToString(withoutDef));

    // objects are kept for the second save
    CHECK(withoutDef.find("N[") != std::string::npos);
    CHECK(withoutDef.find("position") != std::string::npos);

    OTN::OTNReader r1;
    CHECK(r1.ReadString(withDef));
    OTN::OTNReader r2;
    CHECK(r2.ReadString(withoutDef));
    CHECK(r2.TryGetObject("M").has_value());

    // ClearObjects empties the writer
    std::string empty;
    CHECK(w.ClearObjects().SaveToString(empty));
    CHECK(empty.find("N[") == std::string::npos);
}

// ============================================================
//  StreamWriter: BeginObject with an already written name
// ============================================================

inline void Test_Bugfix_StreamBeginObjectTwice(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "bugfix_stream_begin_twice");

    OTN::OTNStreamWriter sw;
    CHECK(sw.Open(path));
    CHECK(sw.BeginObject("X", OTN::col("v", "int")));
    CHECK(sw.WriteRow(1));
    CHECK(sw.EndObject());
    CHECK(!sw.BeginObject("X", OTN::col("v", "int")));
    sw.Close();

    OTN::OTNReader r;
    CHECK(r.ReadFile(path));
}

// ============================================================
//  'any' columns: numbers keep their precision and can be read as every numeric type
// ============================================================

inline void Test_Bugfix_AnyNumbers(const std::filesystem::path& dir) {
    const double d = 0.1234567890123;
    const int64_t i64 = 5000000000LL;
    const uint64_t u64 = 18000000000000000000ULL;
    const float f = 0.1f;

    OTN::OTNObject obj("AnyNum");
    obj.SetNames("d", "i64", "u64", "f", "i", "neg", "list")
        .SetTypes("any", "any", "any", "any", "any", "any", "any[]")
        .AddDataRow(d, i64, u64, f, 3, -7, std::vector<float>{ 1.5f, 2.0f });

    OTN::OTNWriter w;
    w.AppendObject(obj);
    std::string text;
    CHECK(w.SaveToString(text));

    OTN::OTNReader r;
    CHECK(r.ReadString(text));
    auto read = r.TryGetObject("AnyNum");
    CHECK(read.has_value());
    if (!read)
        return;

    CHECK_EQ(read->GetValue<double>(0, "d", 0.0), d);
    CHECK_EQ(read->GetValue<int64_t>(0, "i64", 0), i64);
    CHECK_EQ(read->GetValue<uint64_t>(0, "u64", 0), u64);
    CHECK_EQ(read->GetValue<float>(0, "f", 0.0f), f);     // stored as double, read as float
    CHECK_EQ(read->GetValue<int>(0, "i", 0), 3);
    CHECK_EQ(read->GetValue<int64_t>(0, "i", 0), int64_t(3)); // int read as int64
    CHECK_EQ(read->GetValue<double>(0, "i", 0.0), 3.0);     // int read as double
    CHECK_EQ(read->GetValue<int>(0, "neg", 0), -7);
    CHECK(!read->TryGetValue<uint64_t>(0, "neg").has_value()); // out of range
    CHECK(!read->TryGetValue<int>(0, "i64").has_value());      // out of range
    CHECK(!read->TryGetValue<int>(0, "d").has_value());        // not integral

    auto list = read->TryGetValue<std::vector<float>>(0, "list");
    CHECK(list.has_value());
    if (list && list->size() == 2)
        CHECK_EQ((*list)[0], 1.5f);

    // only 'any' columns convert numbers
    OTN::OTNObject typed("Typed");
    typed.SetNames("d").AddDataRow(d);
    CHECK(!typed.TryGetValue<float>(0, "d").has_value());

    // stream reader parses 'any' numbers the same way
    auto path = MakeTestFile(dir, "bugfix_any_numbers");
    OTN::OTNStreamWriter sw;
    CHECK(sw.Open(path));
    CHECK(sw.AppendObject(obj));
    CHECK(sw.Close());

    OTN::OTNStreamReader sr;
    CHECK(sr.Open(path));
    CHECK(sr.NextObject());
    OTN::OTNRow row;
    CHECK(sr.ReadRow(row));
    if (row.size() == 7) {
        CHECK(row[0].type == OTN::OTNBaseType::DOUBLE);
        CHECK(row[1].type == OTN::OTNBaseType::INT64);
        CHECK(row[2].type == OTN::OTNBaseType::UINT64);
    }
}

// ============================================================
//  Row deduplication with 'any' column
// ============================================================

inline void Test_Bugfix_DedupAny(const std::filesystem::path&) {
    OTN::OTNObject obj("DedupAny");
    obj.SetNames("a").SetTypes("any").AddDataRow(1).AddDataRow(1).AddDataRow(std::string("x"));

    OTN::OTNWriter w;
    w.UseDeduplicateRows(true).AppendObject(obj);
    std::string text;
    CHECK(w.SaveToString(text));

    OTN::OTNReader r;
    CHECK(r.ReadString(text));
    auto read = r.TryGetObject("DedupAny");
    CHECK(read.has_value());
    if (read)
        CHECK_EQ(read->GetRowCount(), size_t(2));
}

// ============================================================
//  Reader: invalid numbers do not throw
// ============================================================

inline void Test_Bugfix_InvalidNumbers(const std::filesystem::path& dir) {
    OTN::OTNReader r;
    CHECK(!r.ReadString("@version: 99999999999;\n@object: {\n};"));

    OTN::OTNReader r2;
    CHECK(!r2.ReadString("@version: 257;\n@object: {\n};"));

    const std::string negUInt = "@version: 1;\n@object: {\nU[1] {\nuint64/a\n};\n-1;\n};";
    OTN::OTNReader r3;
    CHECK(!r3.ReadString(negUInt));

    auto path = MakeTestFile(dir, "bugfix_negative_uint64.otn");
    {
        std::ofstream file(path, std::ios::binary);
        file << negUInt;
    }

    OTN::OTNStreamReader sr;
    CHECK(sr.Open(path));
    CHECK(sr.NextObject());
    OTN::OTNRow row;
    CHECK(!sr.ReadRow(row));

    std::string defTypeOverflow =
        "@version: 1;\n@defType: int = 0;\n@object: {\nX[0] {\n99999999999/a\n};\n};";
    OTN::OTNReader r4;
    CHECK(!r4.ReadString(defTypeOverflow));

    // object reference index larger than size_t
    std::string refOverflow =
        "@version: 1;\n@object: {\nT[1] {\nint/v\n};\n1;\n"
        "P[1] {\nRef<T>/t\n};\n99999999999999999999999;\n};";
    OTN::OTNReader r5;
    CHECK(!r5.ReadString(refOverflow));

    // index above the int range is parsed (and reported as out of bounds)
    std::string refBig =
        "@version: 1;\n@object: {\nT[1] {\nint/v\n};\n1;\n"
        "P[1] {\nRef<T>/t\n};\n5000000000;\n};";
    OTN::OTNReader r6;
    CHECK(!r6.ReadString(refBig));
    CHECK(r6.GetError().find("index 5000000000") != std::string::npos);
}

// ============================================================
//  StreamWriter can be reused for multiple files
// ============================================================

inline void Test_Bugfix_StreamWriterReuse(const std::filesystem::path& dir) {
    auto pathA = MakeTestFile(dir, "bugfix_stream_reuse_a");
    auto pathB = MakeTestFile(dir, "bugfix_stream_reuse_b");

    OTN::OTNObject a("Reuse");
    a.SetNames("v").AddDataRow(1);
    OTN::OTNObject b("Reuse");
    b.SetNames("v").AddDataRow(2);

    OTN::OTNStreamWriter sw;
    CHECK(sw.Open(pathA));
    CHECK(sw.AppendObject(a));
    CHECK(sw.Close());

    CHECK(sw.Open(pathB));
    CHECK(sw.AppendObject(b));
    CHECK(sw.Close());

    OTN::OTNReader r;
    CHECK(r.ReadFile(pathB));
    auto read = r.TryGetObject("Reuse");
    CHECK(read.has_value());
    if (read)
        CHECK_EQ(read->GetValue<int>(0, "v", 0), 2);
}

// ============================================================
//  Builder: AddNameData with rvalues, uint32_t and OTNObject members
// ============================================================

inline void Test_Bugfix_BuilderTypes(const std::filesystem::path&) {
    OTN::OTNObject obj("BuilderTypes");
    obj.SetNames("nd", "u", "om")
        .AddDataRow(NameDataType{ 4 }, UIntType{ 4000000000u }, ObjectMemberType{});
    CHECK(obj.IsValid());

    OTN::OTNWriter w;
    w.AppendObject(obj);
    std::string text;
    CHECK(w.SaveToString(text));

    OTN::OTNReader r;
    CHECK(r.ReadString(text));
    auto read = r.TryGetObject("BuilderTypes");
    CHECK(read.has_value());
    if (!read)
        return;

    auto nd = read->TryGetValue<NameDataType>(0, "nd");
    CHECK(nd.has_value());
    if (nd)
        CHECK_EQ(nd->v, 4);

    auto u = read->TryGetValue<UIntType>(0, "u");
    CHECK(u.has_value());
    if (u)
        CHECK_EQ(u->v, 4000000000u);
}

// ============================================================
//  Entry point — call all regression tests
// ============================================================

inline void RunBugfixTests(const std::filesystem::path& basePath) {
    std::cout << "\n--- Bugfix Regression Tests ---\n";
    RUN(Test_Bugfix_StringEscape);
    RUN(Test_Bugfix_DefTypeList);
    RUN(Test_Bugfix_StreamAddressReuse);
    RUN(Test_Bugfix_StreamSharedSubObject);
    RUN(Test_Bugfix_AddDataRowInvalidFirstRow);
    RUN(Test_Bugfix_StreamObjectWithoutTypes);
    RUN(Test_Bugfix_StreamMultiRowNested);
    RUN(Test_Bugfix_CopyAssignmentCache);
    RUN(Test_Bugfix_WriterMultipleSaves);
    RUN(Test_Bugfix_StreamBeginObjectTwice);
    RUN(Test_Bugfix_AnyNumbers);
    RUN(Test_Bugfix_DedupAny);
    RUN(Test_Bugfix_InvalidNumbers);
    RUN(Test_Bugfix_StreamWriterReuse);
    RUN(Test_Bugfix_BuilderTypes);
}
