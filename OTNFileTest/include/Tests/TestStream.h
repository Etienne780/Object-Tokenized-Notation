#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <OTNFile/OTNFile.h>
#include "TestFramework.h"
#include "TestHelpers.h"

// ============================================================
//  Basic StreamWriter → StreamReader round-trip
// ============================================================

inline void Test_Stream_BasicRoundTrip(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "stream_rw");

    // --- Write ---
    OTN::OTNStreamWriter sw;
    CHECK(sw.Open(path));
    CHECK(sw.BeginObject("StreamObj",
            OTN::col("id",   "int"),
            OTN::col("name", "String"),
            OTN::col("val",  "float")));

    CHECK(sw.WriteRow(1, std::string("Alpha"), 1.1f));
    CHECK(sw.WriteRow(2, std::string("Beta"), 2.2f));
    CHECK(sw.WriteRow(3, std::string("Gamma"), 3.3f));
    CHECK(sw.EndObject());
    CHECK(sw.Close());
    CHECK(sw.IsValid());

    // --- Read ---
    OTN::OTNStreamReader sr;
    CHECK(sr.Open(path));
    CHECK(sr.HasMoreObjects());

    bool gotObj = sr.NextObject();
    CHECK(gotObj);
    if (!gotObj) 
        return;

    CHECK_EQ(sr.GetCurrentObjectName(), std::string("StreamObj"));
    CHECK_EQ(sr.GetRemainingRowCount(), size_t(3));

    OTN::OTNRow row;
    int rowCount = 0;
    
    while (sr.ReadRow(row)) {
        CHECK_EQ(row.size(), size_t(3));
        rowCount++;
    }

    CHECK_EQ(rowCount, 3);
    CHECK(!sr.HasMoreObjects());
}

// ============================================================
//  Multiple objects written and read sequentially
// ============================================================

inline void Test_Stream_MultipleObjects(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "stream_multi");

    OTN::OTNStreamWriter sw;
    CHECK(sw.Open(path));

    sw.BeginObject("Obj1", OTN::col("v", "int"));
    sw.WriteRow(10);
    sw.WriteRow(20);
    sw.EndObject();

    sw.BeginObject("Obj2", OTN::col("s", "String"));
    sw.WriteRow(std::string("hello"));
    sw.EndObject();

    CHECK(sw.Close());

    OTN::OTNStreamReader sr;
    sr.Open(path);

    CHECK(sr.NextObject());
    CHECK_EQ(sr.GetCurrentObjectName(), std::string("Obj1"));
    CHECK_EQ(sr.GetRemainingRowCount(), size_t(2));

    OTN::OTNRow row;
    sr.ReadRow(row); sr.ReadRow(row); // consume Obj1

    CHECK(sr.NextObject());
    CHECK_EQ(sr.GetCurrentObjectName(), std::string("Obj2"));
    CHECK_EQ(sr.GetRemainingRowCount(), size_t(1));
}

// ============================================================
//  SkipCurrentObject — advance without reading all rows
// ============================================================

inline void Test_Stream_SkipObject(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "stream_skip");

    OTN::OTNStreamWriter sw;
    sw.Open(path);

    sw.BeginObject("Obj1", OTN::col("v", "int"));
    for (int i = 0; i < 5; ++i) 
        sw.WriteRow(i);
    sw.EndObject();

    sw.BeginObject("Obj2", OTN::col("v", "int"));
    sw.WriteRow(999);
    sw.EndObject();

    sw.Close();

    OTN::OTNStreamReader sr;
    sr.Open(path);

    sr.NextObject();
    CHECK_EQ(sr.GetCurrentObjectName(), std::string("Obj1"));
    CHECK(sr.SkipCurrentObject());   // skip without reading

    CHECK(sr.NextObject());
    CHECK_EQ(sr.GetCurrentObjectName(), std::string("Obj2"));

    OTN::OTNRow row;
    CHECK(sr.ReadRow(row));
    CHECK_EQ(row.size(), size_t(1));
}

// ============================================================
//  HasMoreRows / HasMoreObjects state transitions
// ============================================================

inline void Test_Stream_HasMoreRowsObjects(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "stream_states");

    OTN::OTNStreamWriter sw;
    sw.Open(path);
    sw.BeginObject("Single", OTN::col("n", "int"));
    sw.WriteRow(1);
    sw.WriteRow(2);
    sw.EndObject();
    sw.Close();

    OTN::OTNStreamReader sr;
    sr.Open(path);

    CHECK(sr.HasMoreObjects());
    sr.NextObject();
    CHECK(sr.HasMoreRows());

    OTN::OTNRow row;
    sr.ReadRow(row);
    CHECK(sr.HasMoreRows());   // one more left

    sr.ReadRow(row);
    CHECK(!sr.HasMoreRows());  // all consumed
    CHECK(!sr.HasMoreObjects());
}

// ============================================================
//  GetWrittenRowCount tracks rows per object name
// ============================================================

inline void Test_Stream_GetWrittenRowCount(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "stream_rowcount");

    OTN::OTNStreamWriter sw;
    sw.Open(path);

    CHECK_EQ(sw.GetWrittenRowCount("MyObj"), size_t(0));

    sw.BeginObject("MyObj", OTN::col("v", "int"));
    sw.WriteRow(1);
    sw.WriteRow(2);
    sw.EndObject();

    CHECK_EQ(sw.GetWrittenRowCount("MyObj"), size_t(2));
    sw.Close();
}

// ============================================================
//  BeginObject vector-of-strings overload
// ============================================================

inline void Test_Stream_VectorOverload(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "stream_vec_overload");

    OTN::OTNStreamWriter sw;
    sw.Open(path);
    CHECK(sw.BeginObject("VecObj",
            std::vector<std::string>{"a", "b"},
            std::vector<std::string>{"int", "float"}));
    sw.WriteRow(5, 1.5f);
    sw.EndObject();
    sw.Close();

    OTN::OTNStreamReader sr;
    sr.Open(path);
    sr.NextObject();
    CHECK_EQ(sr.GetCurrentObjectName(), std::string("VecObj"));
    CHECK_EQ(sr.GetRemainingRowCount(), size_t(1));
}

// ============================================================
//  StreamReader — version available after Open
// ============================================================

inline void Test_Stream_Version(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "stream_version");

    OTN::OTNStreamWriter sw;
    sw.Open(path);
    sw.BeginObject("V", OTN::col("n", "int"));
    sw.WriteRow(0);
    sw.EndObject();
    sw.Close();

    OTN::OTNStreamReader sr;
    sr.Open(path);
    CHECK(sr.GetVersion() > 0);
}

// ============================================================
//  StreamReader — GetColumnNames / GetColumnTypes
// ============================================================

inline void Test_Stream_ColumnMeta(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "stream_col_meta");

    OTN::OTNStreamWriter sw;
    sw.Open(path);
    sw.BeginObject("Meta",
        OTN::col("id", "int"),
        OTN::col("tag", "String"));
    sw.WriteRow(1, std::string("x"));
    sw.EndObject();
    sw.Close();

    OTN::OTNStreamReader sr;
    sr.Open(path);
    sr.NextObject();

    const auto& names = sr.GetColumnNames();
    const auto& types = sr.GetColumnTypes();
    CHECK_EQ(names.size(), size_t(2));
    CHECK_EQ(types.size(), size_t(2));
    CHECK_EQ(names[0], std::string("id"));
    CHECK_EQ(names[1], std::string("tag"));
}

// ============================================================
//  StreamWriter AppendObject (high-level)
// ============================================================

inline void Test_Stream_AppendObject(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "stream_append");

    OTN::OTNObject obj("AppObj");
    obj.SetNames("n", "v")
       .AddDataRow(1, 1.1f)
       .AddDataRow(2, 2.2f);

    OTN::OTNStreamWriter sw;
    sw.Open(path);
    CHECK(sw.AppendObject(obj));
    CHECK(sw.Close());

    OTN::OTNStreamReader sr;
    sr.Open(path);
    CHECK(sr.NextObject());
    CHECK_EQ(sr.GetRemainingRowCount(), size_t(2));
}

// ============================================================
//  StreamWriter writes the exact row count, temp file is removed
// ============================================================

inline std::string ReadTextFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

inline void Test_Stream_WriterRowCountHeader(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "stream_rowcount_header.otn");
    auto tempPath = path;
    tempPath.replace_extension(OTN::TEMP_FILE_EXTENSION);

    OTN::OTNStreamWriter sw;
    CHECK(sw.Open(path));
    CHECK(sw.BeginObject("Counted", OTN::col("v", "int")));
    CHECK(sw.WriteRow(1));
    CHECK(sw.WriteRow(2));
    CHECK(sw.WriteRow(3));
    CHECK(std::filesystem::exists(tempPath));
    CHECK_EQ(tempPath.filename().string(), std::string("stream_rowcount_header.otn.tmp"));
    CHECK(sw.EndObject());
    CHECK(std::filesystem::exists(tempPath)); // rows stay until Close
    CHECK(sw.Close());
    CHECK(!std::filesystem::exists(tempPath));

    std::string text = ReadTextFile(path);
    CHECK(text.find("Counted[3]") != std::string::npos);
    CHECK(text.find("000000003") == std::string::npos);

    OTN::OTNReader r;
    CHECK(r.ReadFile(path));
    auto obj = r.TryGetObject("Counted");
    CHECK(obj.has_value());
    if (obj)
        CHECK_EQ(obj->GetRowCount(), size_t(3));
}

// ============================================================
//  StreamReader: random access to objects and rows
// ============================================================

inline std::filesystem::path WriteParentWithVec2(const std::filesystem::path& dir, const std::string& name) {
    auto path = MakeTestFile(dir, name);

    OTN::OTNObject parent("Parent");
    parent.SetNames("id", "pos")
        .AddDataRow(1, Vec2{ 1.0f, 2.0f })
        .AddDataRow(2, Vec2{ 3.0f, 4.0f });

    OTN::OTNStreamWriter sw;
    sw.Open(path);
    sw.AppendObject(parent);
    sw.Close();
    return path;
}

inline void Test_Stream_ReadObjectRow(const std::filesystem::path& dir) {
    auto path = WriteParentWithVec2(dir, "stream_read_object_row");

    OTN::OTNStreamReader sr;
    CHECK(sr.Open(path));

    auto names = sr.GetObjectNames();
    CHECK_EQ(names.size(), size_t(2));

    auto vec = sr.ReadObjectRow("Vec2", 1);
    CHECK(vec.has_value());
    if (vec)
        CHECK_EQ(vec->GetValue<float>(0, "x", 0.0f), 3.0f);

    auto parent = sr.ReadObjectRow("Parent", 0);
    CHECK(parent.has_value());
    if (parent) {
        auto pos = parent->TryGetValue<Vec2>(0, "pos");
        CHECK(pos.has_value());
        if (pos)
            CHECK_EQ(pos->y, 2.0f);
    }

    // missing object / row: no value, reader stays valid
    CHECK(!sr.ReadObjectRow("Missing", 0).has_value());
    CHECK(!sr.ReadObjectRow("Vec2", 5).has_value());
    CHECK(sr.IsValid());

    auto whole = sr.ReadObject("Parent");
    CHECK(whole.has_value());
    if (whole) {
        CHECK_EQ(whole->GetRowCount(), size_t(2));
        auto pos = whole->TryGetValue<Vec2>(1, "pos");
        CHECK(pos.has_value());
        if (pos)
            CHECK_EQ(pos->x, 3.0f);
    }
}

// ============================================================
//  StreamReader: ReadRow resolves object references
// ============================================================

inline void Test_Stream_ReadRowResolvesRefs(const std::filesystem::path& dir) {
    auto path = WriteParentWithVec2(dir, "stream_resolve_refs");

    OTN::OTNStreamReader sr;
    CHECK(sr.Open(path));
    CHECK(sr.GetResolveObjectRefs());

    while (sr.NextObject() && sr.GetCurrentObjectName() != "Parent") {}
    CHECK_EQ(sr.GetCurrentObjectName(), std::string("Parent"));

    OTN::OTNRow row;
    CHECK(sr.ReadRow(row));
    if (row.size() == 2)
        CHECK(row[1].type == OTN::OTNBaseType::OBJECT);

    // sequential reading continues after resolving
    CHECK(sr.ReadRow(row));
    if (row.size() == 2)
        CHECK_EQ(std::get<int>(row[0].value), 2);
    CHECK(!sr.HasMoreRows());

    // unresolved references
    OTN::OTNStreamReader sr2;
    sr2.SetResolveObjectRefs(false);
    CHECK(sr2.Open(path));
    while (sr2.NextObject() && sr2.GetCurrentObjectName() != "Parent") {}
    CHECK(sr2.ReadRow(row));
    if (row.size() == 2)
        CHECK(row[1].type == OTN::OTNBaseType::OBJECT_REF);
}

// ============================================================
//  StreamReader: rows behind several checkpoints
// ============================================================

inline void Test_Stream_LargeObjectRandomAccess(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "stream_large");
    const int rowCount = 5000;

    OTN::OTNStreamWriter sw;
    CHECK(sw.Open(path));
    CHECK(sw.BeginObject("Big", OTN::col("v", "int")));
    for (int i = 0; i < rowCount; ++i)
        sw.WriteRow(i);
    CHECK(sw.EndObject());
    CHECK(sw.Close());

    OTN::OTNStreamReader sr;
    CHECK(sr.Open(path));

    for (size_t index : { size_t(0), size_t(1023), size_t(1024), size_t(2049), size_t(4999) }) {
        auto obj = sr.ReadObjectRow("Big", index);
        CHECK(obj.has_value());
        if (obj)
            CHECK_EQ(obj->GetValue<int>(0, "v", -1), static_cast<int>(index));
    }

    // random access between sequential reads keeps the read position
    CHECK(sr.NextObject());
    OTN::OTNRow row;
    int expected = 0;
    bool allMatch = true;
    while (sr.ReadRow(row)) {
        if (std::get<int>(row[0].value) != expected)
            allMatch = false;
        ++expected;

        if (expected == 10)
            CHECK(sr.ReadObjectRow("Big", 3000).has_value());
    }
    CHECK(allMatch);
    CHECK_EQ(expected, rowCount);
}

// ============================================================
//  StreamReader: file changed while reading -> rescan
// ============================================================

inline void Test_Stream_FileChangedRescan(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "stream_changed.otn");

    auto write = [&](bool withZero, int firstRows, bool withFirst) {
        OTN::OTNStreamWriter sw;
        sw.Open(path);
        if (withZero) {
            sw.BeginObject("Zero", OTN::col("v", "int"));
            sw.WriteRow(0);
            sw.EndObject();
        }
        if (withFirst) {
            sw.BeginObject("First", OTN::col("v", "int"));
            for (int i = 1; i <= firstRows; ++i)
                sw.WriteRow(i);
            sw.EndObject();
        }
        sw.BeginObject("Second", OTN::col("v", "int"));
        sw.WriteRow(9);
        sw.EndObject();
        sw.Close();
    };

    write(false, 3, true);

    OTN::OTNStreamReader sr;
    CHECK(sr.Open(path));
    CHECK(sr.NextObject());
    CHECK_EQ(sr.GetCurrentObjectName(), std::string("First"));

    OTN::OTNRow row;
    CHECK(sr.ReadRow(row));
    CHECK_EQ(std::get<int>(row[0].value), 1);

    // other layout and size: ReadRow does not check the file, ReadObjectRow does.
    // The reader rescans and continues at First row 1
    write(true, 4, true);
    std::optional<OTN::OTNObject> last = sr.ReadObjectRow("First", 3);
    CHECK(last.has_value());
    if (last)
        CHECK_EQ(last->GetValue<int>(0, "v", 0), 4);

    CHECK(sr.ReadRow(row));
    if (!row.empty())
        CHECK_EQ(std::get<int>(row[0].value), 2);
    CHECK_EQ(sr.GetRemainingRowCount(), size_t(2));
    CHECK(sr.IsValid());

    CHECK(sr.NextObject());
    CHECK_EQ(sr.GetCurrentObjectName(), std::string("Second"));

    // current object removed -> error
    OTN::OTNStreamReader sr2;
    write(false, 3, true);
    CHECK(sr2.Open(path));
    CHECK(sr2.NextObject());
    write(true, 0, false);
    CHECK(!sr2.NextObject());
    CHECK(!sr2.IsValid());
}

// ============================================================
//  StreamReader: cyclic object references
// ============================================================

inline void Test_Stream_CyclicReference(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "stream_cyclic.otn");
    {
        std::ofstream file(path, std::ios::binary);
        file << "@version: 1;\n@object: {\n"
             << "A[1] {\nRef<B>/b\n};\n0;\n"
             << "B[1] {\nRef<A>/a\n};\n0;\n};\n";
    }

    OTN::OTNStreamReader sr;
    CHECK(sr.Open(path));
    CHECK(!sr.ReadObjectRow("A", 0).has_value());
    CHECK(sr.GetError().find("cyclic") != std::string::npos);
}

// ============================================================
//  StreamWriter: WriteRow with custom types
// ============================================================

inline void Test_Stream_WriteRowCustomType(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "stream_writerow_custom.otn");
    auto tempPath = path;
    tempPath.replace_extension(OTN::TEMP_FILE_EXTENSION);

    OTN::OTNStreamWriter sw;
    CHECK(sw.Open(path));
    CHECK(sw.BeginObject("Enemy", OTN::col("hp", "int"), OTN::col("pos", "Vec2"), OTN::col("path", "Vec2[]")));
    CHECK(sw.WriteRow(30, Vec2{ 1.0f, 2.0f }, std::vector<Vec2>{ { 3.0f, 4.0f }, { 5.0f, 6.0f } }));
    CHECK(sw.WriteRow(60, Vec2{ 7.0f, 8.0f }, std::vector<Vec2>{}));
    CHECK(sw.EndObject());

    // Vec2 is used again: rows are appended to the same object
    CHECK(sw.BeginObject("Vec2", OTN::col("x", "float"), OTN::col("y", "float")));
    CHECK(sw.WriteRow(9.0f, 10.0f));
    CHECK(sw.EndObject());
    CHECK_EQ(sw.GetWrittenRowCount("Vec2"), size_t(5));
    CHECK(sw.Close());
    CHECK(!std::filesystem::exists(tempPath));

    OTN::OTNReader r;
    CHECK(r.ReadFile(path));
    auto enemy = r.TryGetObject("Enemy");
    CHECK(enemy.has_value());
    if (enemy) {
        CHECK_EQ(enemy->GetRowCount(), size_t(2));
        auto pos = enemy->TryGetValue<Vec2>(1, "pos");
        CHECK(pos.has_value());
        if (pos)
            CHECK_EQ(pos->x, 7.0f);

        auto enemyPath = enemy->TryGetValue<std::vector<Vec2>>(0, "path");
        CHECK(enemyPath.has_value());
        if (enemyPath) {
            CHECK_EQ(enemyPath->size(), size_t(2));
            if (enemyPath->size() == 2)
                CHECK_EQ((*enemyPath)[1].y, 6.0f);
        }
    }

    auto vec = r.TryGetObject("Vec2");
    CHECK(vec.has_value());
    if (vec) {
        CHECK_EQ(vec->GetRowCount(), size_t(5));
        CHECK_EQ(vec->GetValue<float>(4, "x", 0.0f), 9.0f);
    }
}

// ============================================================
//  StreamWriter: same object name in multiple calls
// ============================================================

inline void Test_Stream_SameNameAppended(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "stream_same_name");

    OTN::OTNObject a("Item");
    a.SetNames("v").AddDataRow(1);
    OTN::OTNObject b("Item");
    b.SetNames("v").AddDataRow(2).AddDataRow(3);

    OTN::OTNStreamWriter sw;
    CHECK(sw.Open(path));
    CHECK(sw.AppendObject(a));
    CHECK(sw.BeginObject("Other", OTN::col("s", "String")));
    CHECK(sw.WriteRow(std::string("x")));
    CHECK(sw.EndObject());
    CHECK(sw.AppendObject(b));
    CHECK(sw.BeginObject("Item", OTN::col("v", "int")));
    CHECK(sw.WriteRow(4));
    CHECK(sw.EndObject());
    CHECK(sw.Close());

    OTN::OTNReader r;
    CHECK(r.ReadFile(path));
    auto item = r.TryGetObject("Item");
    CHECK(item.has_value());
    if (item) {
        CHECK_EQ(item->GetRowCount(), size_t(4));
        for (size_t i = 0; i < item->GetRowCount(); ++i)
            CHECK_EQ(item->GetValue<int>(i, "v", 0), static_cast<int>(i + 1));
    }
    CHECK(r.TryGetObject("Other").has_value());

    // empty list has no element type, still matches the existing int[] column
    OTN::OTNObject full("List");
    full.SetNames("v").AddDataRow(std::vector<int>{ 1, 2 });
    OTN::OTNObject empty("List");
    empty.SetNames("v").AddDataRow(std::vector<int>{});

    OTN::OTNStreamWriter swList;
    CHECK(swList.Open(path));
    CHECK(swList.AppendObject(full));
    CHECK(swList.AppendObject(empty));
    CHECK(swList.Close());

    OTN::OTNReader rList;
    CHECK(rList.ReadFile(path));
    auto list = rList.TryGetObject("List");
    CHECK(list.has_value());
    if (list)
        CHECK_EQ(list->GetRowCount(), size_t(2));

    // other columns for an existing name -> error
    OTN::OTNStreamWriter sw2;
    CHECK(sw2.Open(path));
    CHECK(sw2.AppendObject(a));
    CHECK(!sw2.BeginObject("Item", OTN::col("v", "float")));
    CHECK(!sw2.IsValid());
    sw2.Close();
}

// ============================================================
//  Entry point — call all stream tests
// ============================================================

inline void RunStreamTests(const std::filesystem::path& basePath) {
    std::cout << "\n--- OTNStreamWriter / OTNStreamReader Tests ---\n";
    RUN(Test_Stream_BasicRoundTrip);
    RUN(Test_Stream_MultipleObjects);
    RUN(Test_Stream_SkipObject);
    RUN(Test_Stream_HasMoreRowsObjects);
    RUN(Test_Stream_GetWrittenRowCount);
    RUN(Test_Stream_VectorOverload);
    RUN(Test_Stream_Version);
    RUN(Test_Stream_ColumnMeta);
    RUN(Test_Stream_AppendObject);
    RUN(Test_Stream_WriterRowCountHeader);
    RUN(Test_Stream_ReadObjectRow);
    RUN(Test_Stream_ReadRowResolvesRefs);
    RUN(Test_Stream_LargeObjectRandomAccess);
    RUN(Test_Stream_FileChangedRescan);
    RUN(Test_Stream_CyclicReference);
    RUN(Test_Stream_WriteRowCustomType);
    RUN(Test_Stream_SameNameAppended);
}
