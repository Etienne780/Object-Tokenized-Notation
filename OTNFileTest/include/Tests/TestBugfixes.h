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
//  StreamWriter: BeginObject with an already written name appends the rows
// ============================================================

inline void Test_Bugfix_StreamBeginObjectTwice(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "bugfix_stream_begin_twice");

    OTN::OTNStreamWriter sw;
    CHECK(sw.Open(path));
    CHECK(sw.BeginObject("X", OTN::col("v", "int")));
    CHECK(sw.WriteRow(1));
    CHECK(sw.EndObject());
    CHECK(sw.BeginObject("X", OTN::col("v", "int")));
    CHECK(sw.WriteRow(2));
    CHECK(sw.Close());

    OTN::OTNReader r;
    CHECK(r.ReadFile(path));
    auto x = r.TryGetObject("X");
    CHECK(x.has_value());
    if (x)
        CHECK_EQ(x->GetRowCount(), size_t(2));
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
//  OTNReader::FindObject returns the stored object without copy
// ============================================================

inline void Test_Bugfix_ReaderFindObject(const std::filesystem::path&) {
    OTN::OTNObject obj("Find");
    obj.SetNames("v").AddDataRow(1).AddDataRow(2);

    OTN::OTNWriter w;
    std::string text;
    CHECK(w.AppendObject(obj).SaveToString(text));

    OTN::OTNReader r;
    CHECK(r.ReadString(text));
    CHECK(r.FindObject("Missing") == nullptr);

    OTN::OTNObject* found = r.FindObject("Find");
    CHECK(found != nullptr);
    if (!found)
        return;

    CHECK_EQ(found->GetRowCount(), size_t(2));
    found->AddDataRow(3);

    const OTN::OTNReader& cr = r;
    const OTN::OTNObject* constFound = cr.FindObject("Find");
    CHECK(constFound == found);
    CHECK_EQ(r.TryGetObject("Find")->GetRowCount(), size_t(3));
}

// ============================================================
//  OTNObjectBuilder: explicit types (empty lists, any)
// ============================================================

struct TaggedValue {
    std::vector<std::string> tags;
    double value = 0;
    int id = 0;
};

template<>
inline void OTN::ToOTNDataType<TaggedValue>(OTN::OTNObjectBuilder& b, TaggedValue& t) {
    b.SetObjectName("TaggedValue");
    b.AddNames("tags", "value");
    b.AddTypes("String[]", "any");
    b.AddData(t.tags, t.value);
    b.AddNameTypeData("id", "-", t.id); // "-" = deduced
}

struct WrongTypedValue {
    float v = 1.5f;
};

template<>
inline void OTN::ToOTNDataType<WrongTypedValue>(OTN::OTNObjectBuilder& b, WrongTypedValue& w) {
    b.SetObjectName("WrongTypedValue");
    b.AddNameTypeData("v", "int", w.v);
}

inline void Test_Bugfix_BuilderExplicitTypes(const std::filesystem::path& dir) {
    OTN::OTNObject obj("Holder");
    obj.SetNames("t")
        .AddDataRow(TaggedValue{ {}, 2.5, 1 })            // empty list first
        .AddDataRow(TaggedValue{ { "a", "b" }, 3.0, 2 });
    CHECK(obj.IsValid());

    auto check = [](const std::optional<OTN::OTNObject>& holder) {
        CHECK(holder.has_value());
        if (!holder)
            return;

        auto first = holder->TryGetValue<TaggedValue>(0, "t");
        auto second = holder->TryGetValue<TaggedValue>(1, "t");
        CHECK(first.has_value() && second.has_value());
        if (!first || !second)
            return;

        CHECK(first->tags.empty());
        CHECK_EQ(second->tags.size(), size_t(2));
        CHECK_EQ(first->value, 2.5);
        CHECK_EQ(second->id, 2);
    };

    // OTNWriter
    OTN::OTNWriter w;
    std::string text;
    CHECK(w.AppendObject(obj).SaveToString(text));
    CHECK(text.find("String[]/tags, any/value, int/id") != std::string::npos);

    OTN::OTNReader r;
    CHECK(r.ReadString(text));
    check(r.TryGetObject("Holder"));

    // OTNStreamWriter
    auto path = MakeTestFile(dir, "bugfix_builder_types");
    OTN::OTNStreamWriter sw;
    CHECK(sw.Open(path));
    CHECK(sw.AppendObject(obj));
    CHECK(sw.Close());

    OTN::OTNReader r2;
    CHECK(r2.ReadFile(path));
    check(r2.TryGetObject("Holder"));

    // type does not match the value
    OTN::OTNObject wrong("Wrong");
    wrong.SetNames("w").AddDataRow(WrongTypedValue{});
    CHECK(!wrong.IsValid());
}

// ============================================================
//  OTNWriter: empty list in the first row gets the type of a later row
// ============================================================

inline void Test_Bugfix_WriterEmptyListFirst(const std::filesystem::path&) {
    OTN::OTNObject obj("Lists");
    obj.SetNames("v")
        .AddDataRow(std::vector<int>{})
        .AddDataRow(std::vector<int>{ 1, 2 });

    OTN::OTNWriter w;
    std::string text;
    CHECK(w.AppendObject(obj).SaveToString(text));
    CHECK(text.find("int[]/v") != std::string::npos);

    OTN::OTNReader r;
    CHECK(r.ReadString(text));
    auto read = r.TryGetObject("Lists");
    CHECK(read.has_value());
    if (read) {
        auto list = read->TryGetValue<std::vector<int>>(1, "v");
        CHECK(list.has_value() && list->size() == 2);
    }
}

// ============================================================
//  Reader and writer can be used again after an error
// ============================================================

inline void Test_Bugfix_ResetAfterError(const std::filesystem::path&) {
    OTN::OTNReader r;
    CHECK(!r.ReadString("@version: 1;\n@object: {\nA[1] {\nint/a\n};\nx;\n};"));
    CHECK(!r.IsValid());
    CHECK(r.ReadString("@version: 1;\n@object: {\nA[1] {\nint/a\n};\n1;\n};"));
    CHECK(r.IsValid());
    CHECK(r.GetError().empty());
    CHECK(r.FindObject("A") != nullptr);

    OTN::OTNObject a("Same");
    a.SetNames("v").AddDataRow(1);

    OTN::OTNWriter w;
    w.UseOptimizations(true);
    w.AppendObject(a).AppendObject(a); // same name twice
    std::string text;
    CHECK(!w.SaveToString(text));
    CHECK(!w.IsValid());

    w.Reset();
    CHECK(w.IsValid());
    CHECK(w.GetError().empty());
    CHECK(w.AppendObject(a).SaveToString(text));
    CHECK(w.GetUseOptimizations()); // options stay
    CHECK(text.find('\n') == std::string::npos);
}

// ============================================================
//  Checks that run in every build (also without OTN_DEBUG)
// ============================================================

inline void Test_Bugfix_AlwaysChecked(const std::filesystem::path&) {
    OTN::OTNObject count("Count");
    count.SetNames("a", "b").AddDataRow(1);
    CHECK(!count.IsValid());

    OTN::OTNObject dup("Dup");
    dup.SetNames("a", "a");
    CHECK(!dup.IsValid());

    OTN::OTNObject invalidChar("Space");
    invalidChar.SetNames("my name");
    CHECK(!invalidChar.IsValid());

    OTN::OTNObject digit("Digit");
    digit.SetNames("1abc");
    CHECK(!digit.IsValid());

    OTN::OTNObject objectName("my object");
    CHECK(!objectName.IsValid());

    // invalid objects are not written
    OTN::OTNWriter w;
    std::string text;
    CHECK(!w.AppendObject(count).SaveToString(text));
}

// ============================================================
//  UTF-8 names, byte order mark, UTF-16 files
// ============================================================

inline void Test_Bugfix_Encoding(const std::filesystem::path& dir) {
    // UTF-8 names as escapes, so the test does not depend on the source encoding of the compiler
    const std::string objectName = "Gr\xC3\xB6\xC3\x9F" "e";     // Groesse
    const std::string columnName = "wert_\xC3\xA4";               // wert_ae
    const std::string japanese = "\xE5\x90\x8D\xE5\x89\x8D";     // name in Japanese
    const std::string greeting = "Gr\xC3\xBC\xC3\x9F" "e";       // Gruesse

    // UTF-8 object and column names can be written and read
    OTN::OTNObject obj(objectName);
    obj.SetNames(columnName, japanese).AddDataRow(1, greeting);
    CHECK(obj.IsValid());

    OTN::OTNWriter w;
    std::string text;
    CHECK(w.AppendObject(obj).SaveToString(text));

    OTN::OTNReader r;
    CHECK(r.ReadString(text));
    auto read = r.TryGetObject(objectName);
    CHECK(read.has_value());
    if (read)
        CHECK_EQ(read->GetValue<std::string>(0, japanese, ""), greeting);

    // the stream reader uses the same tokenizer
    auto path = MakeTestFile(dir, "bugfix_utf8_names");
    OTN::OTNStreamWriter sw;
    CHECK(sw.Open(path));
    CHECK(sw.AppendObject(obj));
    CHECK(sw.Close());

    OTN::OTNStreamReader sr;
    CHECK(sr.Open(path));
    auto row = sr.ReadObjectRow(objectName, 0);
    CHECK(row.has_value());
    if (row)
        CHECK_EQ(row->GetValue<int>(0, columnName, 0), 1);

    // UTF-8 byte order mark is skipped
    OTN::OTNReader bom;
    CHECK(bom.ReadString("\xEF\xBB\xBF" + text));
    CHECK(bom.FindObject(objectName) != nullptr);

    auto bomPath = MakeTestFile(dir, "bugfix_bom.otn");
    {
        std::ofstream file(bomPath, std::ios::binary);
        file << "\xEF\xBB\xBF" << text;
    }
    OTN::OTNStreamReader bomStream;
    CHECK(bomStream.Open(bomPath));
    CHECK(bomStream.ReadObjectRow(objectName, 0).has_value());

    // UTF-16 is rejected with a clear message
    std::string utf16("\xFF\xFE", 2);
    for (char c : std::string("@version: 1;")) {
        utf16 += c;
        utf16 += '\0';
    }
    OTN::OTNReader r16;
    CHECK(!r16.ReadString(utf16));
    CHECK(r16.GetError().find("UTF-8") != std::string::npos);
}

// ============================================================
//  Path errors are reported, not thrown
// ============================================================

inline void Test_Bugfix_PathErrors(const std::filesystem::path& dir) {
    OTN::OTNReader r;
    CHECK(!r.ReadFile(dir / "missing_dir" / "missing.otn"));
    CHECK(!r.IsValid());

    OTN::OTNReader r2;
    CHECK(!r2.ReadFile(dir / "wrong_extension.txt"));
    CHECK(r2.GetError().find("extension") != std::string::npos);

    OTN::OTNStreamReader sr;
    CHECK(!sr.Open(dir / "missing_dir" / "missing.otn"));
}

// ============================================================
//  Version 2: references in 'any' columns (Name#index)
// ============================================================

inline OTN::OTNObject MakeAnyRefObject() {
    OTN::OTNObject items("Item");
    items.SetNames("n").AddDataRow(1).AddDataRow(2);

    OTN::OTNObject obj("AnyRefs");
    obj.SetNames("value").SetTypes("any")
        .AddDataRow(Vec2{ 1.0f, 2.0f })
        .AddDataRow(3.5)
        .AddDataRow(std::vector<Vec2>{ { 3.0f, 4.0f }, { 5.0f, 6.0f } })
        .AddDataRow(items)
        .AddDataRow(std::string("text"));
    return obj;
}

inline void CheckAnyRefObject(const OTN::OTNObject& read) {
    auto pos = read.TryGetValue<Vec2>(0, "value");
    CHECK(pos.has_value());
    if (pos)
        CHECK_EQ(pos->y, 2.0f);

    CHECK_EQ(read.GetValue<double>(1, "value", 0.0), 3.5);

    auto path = read.TryGetValue<std::vector<Vec2>>(2, "value");
    CHECK(path.has_value());
    if (path && path->size() == 2)
        CHECK_EQ((*path)[1].x, 5.0f);
    else
        CHECK(false);

    auto items = read.TryGetValue<std::vector<OTN::OTNObject>>(3, "value");
    CHECK(items.has_value());
    if (items && items->size() == 2)
        CHECK_EQ((*items)[1].GetValue<int>(0, "n", 0), 2);
    else
        CHECK(false);

    CHECK_EQ(read.GetValue<std::string>(4, "value", ""), std::string("text"));
}

inline void Test_V2_AnyReferences(const std::filesystem::path& dir) {
    OTN::OTNObject obj = MakeAnyRefObject();
    CHECK(obj.IsValid());

    OTN::OTNWriter w;
    std::string text;
    CHECK(w.AppendObject(obj).SaveToString(text));
    CHECK(text.find("@version: 2;") != std::string::npos);
    CHECK(text.find("Vec2#0;") != std::string::npos);
    CHECK(text.find("[Vec2#1, Vec2#2]") != std::string::npos);
    CHECK(text.find("[Item#0, Item#1]") != std::string::npos);

    OTN::OTNReader r;
    CHECK(r.ReadString(text));
    if (const OTN::OTNObject* read = r.FindObject("AnyRefs"))
        CheckAnyRefObject(*read);
    else
        CHECK(false);

    // stream writer and both readers
    auto path = MakeTestFile(dir, "v2_any_refs");
    OTN::OTNStreamWriter sw;
    CHECK(sw.Open(path));
    CHECK(sw.AppendObject(obj));
    CHECK(sw.BeginObject("Single", OTN::col("value", "any")));
    CHECK(sw.WriteRow(Vec2{ 7.0f, 8.0f }));
    CHECK(sw.EndObject());
    CHECK(sw.Close());

    OTN::OTNReader r2;
    CHECK(r2.ReadFile(path));
    if (const OTN::OTNObject* read = r2.FindObject("AnyRefs"))
        CheckAnyRefObject(*read);
    else
        CHECK(false);

    OTN::OTNStreamReader sr;
    CHECK(sr.Open(path));
    auto single = sr.ReadObjectRow("Single", 0);
    CHECK(single.has_value());
    if (single) {
        auto pos = single->TryGetValue<Vec2>(0, "value");
        CHECK(pos.has_value());
        if (pos)
            CHECK_EQ(pos->x, 7.0f);
    }
    auto list = sr.ReadObjectRow("AnyRefs", 2);
    CHECK(list.has_value());
    if (list) {
        auto values = list->TryGetValue<std::vector<Vec2>>(0, "value");
        CHECK(values.has_value() && values->size() == 2);
    }
}

// ============================================================
//  Version 2: object names in @defType
// ============================================================

inline void Test_V2_DefTypeObjects(const std::filesystem::path&) {
    OTN::OTNObject obj("Points");
    obj.SetNames("pos", "extra").SetTypes("Vec2", "any");
    // 8 rows: the numbers save more than the @defType entry and directive cost
    for (int i = 0; i < 8; ++i)
        obj.AddDataRow(Vec2{ float(i), 0.0f }, Vec2{ 0.0f, float(i) });

    OTN::OTNWriter w;
    w.UseDefType(true).AppendObject(obj);
    std::string text;
    CHECK(w.SaveToString(text));
    CHECK(text.find("Vec2 = ") != std::string::npos);
    CHECK(text.find("Ref<Vec2>") == std::string::npos); // header uses the number
    CHECK(text.find("#") != std::string::npos);
    CHECK(text.find("Vec2#") == std::string::npos);     // references use the number

    OTN::OTNReader r;
    CHECK(r.ReadString(text));
    const OTN::OTNObject* read = r.FindObject("Points");
    CHECK(read != nullptr);
    if (!read)
        return;

    auto pos = read->TryGetValue<Vec2>(3, "pos");
    auto extra = read->TryGetValue<Vec2>(3, "extra");
    CHECK(pos.has_value() && extra.has_value());
    if (pos && extra) {
        CHECK_EQ(pos->x, 3.0f);
        CHECK_EQ(extra->y, 3.0f);
    }

    // with deduplication equal references are written once
    OTN::OTNObject dup("Dup");
    dup.SetNames("v").SetTypes("any").AddDataRow(Vec2{ 1, 1 }).AddDataRow(Vec2{ 1, 1 });
    OTN::OTNWriter w2;
    w2.UseDeduplicateRows(true).AppendObject(dup);
    std::string dupText;
    CHECK(w2.SaveToString(dupText));
    OTN::OTNReader r2;
    CHECK(r2.ReadString(dupText));
    const OTN::OTNObject* dupRead = r2.FindObject("Dup");
    CHECK(dupRead && dupRead->GetRowCount() == 1);
}

// ============================================================
//  Version 2: version 1 files can still be read, errors
// ============================================================

inline void Test_V2_Compatibility(const std::filesystem::path&) {
    const std::string v1 =
        "@version: 1;\n@defType: float = 0;\n@object: {\n"
        "Vec2[1] {\n0/x, 0/y\n};\n1, 2;\n"
        "P[1] {\nRef<Vec2>/pos, any/a\n};\n0, 3.5;\n};";
    OTN::OTNReader r1;
    CHECK(r1.ReadString(v1));
    CHECK_EQ(r1.GetVersion(), uint8_t(1));
    if (const OTN::OTNObject* p = r1.FindObject("P"))
        CHECK(p->TryGetValue<Vec2>(0, "pos").has_value());
    else
        CHECK(false);

    // references in 'any' and object names in @defType need version 2
    OTN::OTNReader rRef;
    CHECK(!rRef.ReadString("@version: 1;\n@object: {\nV[1] {\nint/x\n};\n1;\nA[1] {\nany/a\n};\nV#0;\n};"));
    OTN::OTNReader rDef;
    CHECK(!rDef.ReadString("@version: 1;\n@defType: V = 0;\n@object: {\n};"));

    const std::string head = "@version: 2;\n@defType: int = 0, V = 1;\n@object: {\nV[1] {\nint/x\n};\n1;\nA[1] {\nany/a\n};\n";
    OTN::OTNReader ok;
    CHECK(ok.ReadString(head + "1#0;\n};"));
    if (const OTN::OTNObject* a = ok.FindObject("A"))
        CHECK_EQ(a->TryGetValue<OTN::OTNObject>(0, "a")->GetValue<int>(0, "x", 0), 1);
    else
        CHECK(false);

    OTN::OTNReader unknownId;
    CHECK(!unknownId.ReadString(head + "5#0;\n};"));
    OTN::OTNReader baseTypeId;
    CHECK(!baseTypeId.ReadString(head + "0#0;\n};"));
    CHECK(baseTypeId.GetError().find("not an object") != std::string::npos);
    OTN::OTNReader badIndex;
    CHECK(!badIndex.ReadString(head + "V#4;\n};"));
    OTN::OTNReader missingIndex;
    CHECK(!missingIndex.ReadString(head + "V;\n};"));
    CHECK(missingIndex.GetError().find("Name#index") != std::string::npos);
}

// ============================================================
//  @defType / @defName entries are only added if they make the file shorter
// ============================================================

inline void Test_Bugfix_DefTablePaysOff(const std::filesystem::path&) {
    auto save = [](const std::vector<OTN::OTNObject>& objects, bool defs, bool compact) {
        OTN::OTNWriter w;
        w.UseDefType(defs).UseDefName(defs).UseOptimizations(compact);
        for (const auto& o : objects)
            w.AppendObject(o);
        std::string text;
        CHECK(w.SaveToString(text));
        return text;
    };

    // 2 x "float" saves less than the entry "float = 0, " costs
    OTN::OTNObject small("Small");
    small.SetNames("a", "b").AddDataRow(1.0f, 2.0f);
    std::string text = save({ small }, true, false);
    CHECK(text.find("@defType") == std::string::npos);
    CHECK(text.find("@defName") == std::string::npos);

    // the same long column name in many objects pays off, the most used name gets number 0
    std::vector<OTN::OTNObject> many;
    for (int i = 0; i < 10; ++i) {
        OTN::OTNObject o("Object" + std::to_string(i));
        if (i < 5)
            o.SetNames("position", "rotation");
        else
            o.SetNames("position", "scale");
        o.AddDataRow(1.0f, 2.0f);
        many.push_back(o);
    }

    for (bool compact : { false, true }) {
        std::string withDefs = save(many, true, compact);
        std::string withoutDefs = save(many, false, compact);
        CHECK(withDefs.size() < withoutDefs.size());
        CHECK(withDefs.find(compact ? "position=0" : "position = 0") != std::string::npos);

        // the result is never longer than without numbers
        std::string smallWith = save({ small }, true, compact);
        std::string smallWithout = save({ small }, false, compact);
        CHECK(smallWith.size() <= smallWithout.size());

        OTN::OTNReader r;
        CHECK(r.ReadString(withDefs));
        const OTN::OTNObject* o = r.FindObject("Object7");
        CHECK(o != nullptr);
        if (o)
            CHECK_EQ(o->GetValue<float>(0, "scale", 0.0f), 2.0f);
    }
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
    RUN(Test_Bugfix_ReaderFindObject);
    RUN(Test_Bugfix_BuilderExplicitTypes);
    RUN(Test_Bugfix_WriterEmptyListFirst);
    RUN(Test_Bugfix_ResetAfterError);
    RUN(Test_Bugfix_AlwaysChecked);
    RUN(Test_Bugfix_Encoding);
    RUN(Test_Bugfix_PathErrors);
    RUN(Test_V2_AnyReferences);
    RUN(Test_V2_DefTypeObjects);
    RUN(Test_V2_Compatibility);
    RUN(Test_Bugfix_DefTablePaysOff);
}
