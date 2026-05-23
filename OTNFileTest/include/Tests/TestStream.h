#pragma once
#include <string>
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
}
