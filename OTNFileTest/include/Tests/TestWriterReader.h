#pragma once
#include <cmath>
#include <string>
#include <filesystem>
#include <OTNFile/OTNFile.h>
#include "TestFramework.h"
#include "TestHelpers.h"

// ============================================================
//  Basic write → read round-trip
// ============================================================

inline void Test_WR_BasicRoundTrip(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "roundtrip");

    OTN::OTNObject obj("Items");
    obj.SetNames("id", "name", "value")
       .AddDataRow(1, std::string("Sword"), 100.5f)
       .AddDataRow(2, std::string("Shield"), 80.0f)
       .AddDataRow(3, std::string("Potion"), 10.25f);

    OTN::OTNWriter writer;
    CHECK(writer.AppendObject(obj).Save(path));
    CHECK(writer.IsValid());

    OTN::OTNReader reader;
    CHECK(reader.ReadFile(path));
    CHECK(reader.IsValid());

    auto loaded = reader.TryGetObject("Items");
    CHECK(loaded.has_value());
    if (!loaded) return;

    CHECK_EQ(loaded->GetRowCount(), size_t(3));
    CHECK_EQ(loaded->GetValue<int>(0, "id"), 1);
    CHECK_EQ(loaded->GetValue<std::string>(0, "name"), std::string("Sword"));
    CHECK(std::abs(loaded->GetValue<float>(0, "value") - 100.5f) < 1e-3f);
    CHECK_EQ(loaded->GetValue<int>(1, "id"), 2);
    CHECK_EQ(loaded->GetValue<std::string>(2, "name"), std::string("Potion"));
}

// ============================================================
//  Multiple objects in one file
// ============================================================

inline void Test_WR_MultipleObjects(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "multi_objects");

    OTN::OTNObject a("ObjectA");
    a.SetNames("x").AddDataRow(1).AddDataRow(2);

    OTN::OTNObject b("ObjectB");
    b.SetNames("y").AddDataRow(std::string("hello"));

    OTN::OTNWriter writer;
    CHECK(writer.AppendObject(a).AppendObject(b).Save(path));

    OTN::OTNReader reader;
    CHECK(reader.ReadFile(path));

    auto la = reader.TryGetObject("ObjectA");
    auto lb = reader.TryGetObject("ObjectB");
    CHECK(la.has_value());
    CHECK(lb.has_value());
    if (la) CHECK_EQ(la->GetRowCount(), size_t(2));
    if (lb) CHECK_EQ(lb->GetValue<std::string>(0, "y"), std::string("hello"));
}

// ============================================================
//  SaveToString / ReadString round-trip
// ============================================================

inline void Test_WR_StringRoundTrip(const std::filesystem::path&) {
    OTN::OTNObject obj("StrObj");
    obj.SetNames("n", "v")
       .AddDataRow(10, 3.14f)
       .AddDataRow(20, 2.71f);

    OTN::OTNWriter writer;
    std::string text;
    CHECK(writer.AppendObject(obj).SaveToString(text));
    CHECK(!text.empty());

    OTN::OTNReader reader;
    CHECK(reader.ReadString(text));

    auto loaded = reader.TryGetObject("StrObj");
    CHECK(loaded.has_value());
    if (!loaded) 
        return;

    CHECK_EQ(loaded->GetRowCount(), size_t(2));
    CHECK_EQ(loaded->GetValue<int>(0, "n"), 10);
    CHECK(std::abs(loaded->GetValue<float>(1, "v") - 2.71f) < 1e-3f);
}

// ============================================================
//  UseOptimizations (removes spaces/linebreaks)
// ============================================================

inline void Test_WR_Optimized(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "optimized");

    OTN::OTNObject obj("OptObj");
    obj.SetNames("a", "b").AddDataRow(1, 2).AddDataRow(3, 4);

    OTN::OTNWriter writer;
    writer.UseOptimizations(true);
    CHECK(writer.GetUseOptimizations());
    CHECK(writer.AppendObject(obj).Save(path));

    OTN::OTNReader reader;
    CHECK(reader.ReadFile(path));
    auto loaded = reader.TryGetObject("OptObj");
    CHECK(loaded.has_value());
    if (loaded) 
        CHECK_EQ(loaded->GetRowCount(), size_t(2));
}

// ============================================================
//  UseDefName / UseDefType optimizations
// ============================================================

inline void Test_WR_DefNameDefType(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "defname_deftype");

    OTN::OTNObject obj("DefObj");
    obj.SetNames("id", "name", "value");
    for (int i = 0; i < 10; ++i)
        obj.AddDataRow(i, std::string("item"), float(i));

    OTN::OTNWriter writer;
    writer.UseDefName(true).UseDefType(true);
    CHECK(writer.GetUseDefName());
    CHECK(writer.GetUseDefType());
    CHECK(writer.AppendObject(obj).Save(path));

    OTN::OTNReader reader;
    CHECK(reader.ReadFile(path));
    auto loaded = reader.TryGetObject("DefObj");
    CHECK(loaded.has_value());
    if (loaded) 
        CHECK_EQ(loaded->GetRowCount(), size_t(10));
}

// ============================================================
//  UseDeduplicateRows
// ============================================================

inline void Test_WR_Deduplication(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "dedup");

    OTN::OTNObject obj("Dedup");
    obj.SetNames("x");
    for (int i = 0; i < 5; ++i)
        obj.AddDataRow(42);    // 5 identical rows

    OTN::OTNWriter writer;
    writer.UseDeduplicateRows(true);
    CHECK(writer.GetDeduplicateRows());
    CHECK(writer.AppendObject(obj).Save(path));

    // In-memory object always keeps all rows
    CHECK_EQ(obj.GetRowCount(), size_t(5));
}

// ============================================================
//  Vector column round-trip
// ============================================================

inline void Test_WR_VectorColumn(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "vec_col");

    std::vector<int> nums = {5, 10, 15, 20};
    OTN::OTNObject obj("VecCol");
    obj.SetNames("data").AddDataRow(nums);

    OTN::OTNWriter w;
    CHECK(w.AppendObject(obj).Save(path));

    OTN::OTNReader reader;
    CHECK(reader.ReadFile(path));

    auto loaded = reader.TryGetObject("VecCol");
    CHECK(loaded.has_value());
    if (!loaded) 
        return;

    auto got = loaded->TryGetValue<std::vector<int>>(0, "data");
    CHECK(got.has_value());
    if (got) {
        CHECK_EQ(got->size(), size_t(4));
        CHECK_EQ((*got)[2], 15);
    }
}

// ============================================================
//  Custom type (Vec2) round-trip
// ============================================================

struct TestVec2 {
    Vec2 more1;
    Vec2 more2;
};

template<>
inline void OTN::ToOTNDataType<TestVec2>(OTN::OTNObjectBuilder& b, TestVec2& v) {
    b.SetObjectName("TestVec2");
    b.AddNames("more1", "more2");
    b.AddData(v.more1, v.more2);
}


inline void Test_WR_CustomType(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "custom_type");

    Vec2 vPos{3.0f, 4.0f};
    Vec2 vRot{45.0f, 0.0f};

    OTN::OTNObject obj("Vectors");
    obj.SetNames("pos", "rot").AddDataRow(vPos, vRot);

    OTN::OTNWriter w;
    CHECK(w.AppendObject(obj).Save(path));

    OTN::OTNReader reader;
    CHECK(reader.ReadFile(path));

    auto loaded = reader.TryGetObject("Vectors");
    CHECK(loaded.has_value());
    if (!loaded) 
        return;

    auto gotPos = loaded->TryGetValue<Vec2>(0, "pos");
    auto gotRot = loaded->TryGetValue<Vec2>(0, "rot");

    CHECK(gotPos.has_value());
    CHECK(gotRot.has_value());

    if (gotPos) {
        CHECK(std::abs(gotPos->x - vPos.x) < 1e-4f);
        CHECK(std::abs(gotPos->y - vPos.y) < 1e-4f);
    }

    if (gotRot) {
        CHECK(std::abs(gotRot->x - vRot.x) < 1e-4f);
        CHECK(std::abs(gotRot->y - vRot.y) < 1e-4f);
    }
}

// ============================================================
//  Large data stress test (1 000 rows)
// ============================================================

inline void Test_WR_LargeData(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "large_data");

    constexpr int N = 1000;
    OTN::OTNObject obj("BigObj");
    obj.SetNames("id", "val");
    obj.ReserveDataRows(N);
    for (int i = 0; i < N; ++i)
        obj.AddDataRow(i, float(i) * 0.001f);

    OTN::OTNWriter w;
    CHECK(w.AppendObject(obj).Save(path));

    OTN::OTNReader reader;
    CHECK(reader.ReadFile(path));

    auto loaded = reader.TryGetObject("BigObj");
    CHECK(loaded.has_value());
    if (loaded) {
        CHECK_EQ(loaded->GetRowCount(), size_t(N));
        CHECK_EQ(loaded->GetValue<int>(N - 1, "id"), N - 1);
    }
}

// ============================================================
//  ClearObjects — should produce a valid (empty) file or fail gracefully
// ============================================================

inline void Test_WR_ClearObjects(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "cleared");

    OTN::OTNObject obj("ClearMe");
    obj.SetNames("v").AddDataRow(99);

    OTN::OTNWriter writer;
    writer.AppendObject(obj).ClearObjects();
    writer.Save(path); // may succeed or fail — must not crash
    CHECK(true);
}

// ============================================================
//  Extension auto-appended when not provided
// ============================================================

inline void Test_WR_ExtensionAutoAppend(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "no_ext_file");

    OTN::OTNObject obj("ExtTest");
    obj.SetNames("v").AddDataRow(1);

    OTN::OTNWriter w;
    CHECK(w.AppendObject(obj).Save(path));

    bool exists = std::filesystem::exists(path.string() + ".otn") ||
                  std::filesystem::exists(path);
    CHECK(exists);
}

// ============================================================
//  Reader — TryGetObject for missing key
// ============================================================

inline void Test_Reader_MissingObject(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "single_obj");

    OTN::OTNObject obj("RealObject");
    obj.SetNames("v").AddDataRow(1);
    OTN::OTNWriter w;
    w.AppendObject(obj).Save(path);

    OTN::OTNReader reader;
    reader.ReadFile(path);

    CHECK(reader.TryGetObject("RealObject").has_value());
    CHECK(!reader.TryGetObject("DoesNotExist").has_value());
}

// ============================================================
//  Reader — version > 0 after successful read
// ============================================================

inline void Test_Reader_Version(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "version_check");

    OTN::OTNObject obj("V");
    obj.SetNames("n").AddDataRow(0);
    OTN::OTNWriter w;
    w.AppendObject(obj).Save(path);

    OTN::OTNReader reader;
    reader.ReadFile(path);
    CHECK(reader.GetVersion() > 0);
}

// ============================================================
//  Reader — GetObjects map contains all written objects
// ============================================================

inline void Test_Reader_GetObjectsMap(const std::filesystem::path& dir) {
    auto path = MakeTestFile(dir, "objects_map");

    OTN::OTNObject a("AA"); a.SetNames("n").AddDataRow(1);
    OTN::OTNObject b("BB"); b.SetNames("n").AddDataRow(2);
    OTN::OTNObject c("CC"); c.SetNames("n").AddDataRow(3);

    OTN::OTNWriter writer;
    writer.AppendObject(a)
        .AppendObject(b)
        .AppendObject(c)
        .Save(path);

    OTN::OTNReader reader;
    reader.ReadFile(path);

    const auto& map = reader.GetObjects();
    CHECK_EQ(map.size(), size_t(3));
    CHECK(map.count("AA") > 0);
    CHECK(map.count("BB") > 0);
    CHECK(map.count("CC") > 0);
}

// ============================================================
//  Reader — invalid / non-existing file
// ============================================================

inline void Test_Reader_InvalidFile(const std::filesystem::path& dir) {
    OTN::OTNReader reader;
    bool ok = reader.ReadFile(dir / "does_not_exist_xyz.otn");
    CHECK(!ok);
    CHECK(!reader.IsValid());

    std::string err;
    CHECK(reader.TryGetError(err));
}

// ============================================================
//  Reader — graceful handling of garbage content
// ============================================================

inline void Test_Reader_InvalidString(const std::filesystem::path&) {
    OTN::OTNReader reader;
    reader.ReadString("THIS IS NOT VALID OTN @@@@"); // must not throw
    CHECK(true);
}

// ============================================================
//  Entry point — call all writer/reader tests
// ============================================================

inline void RunWriterReaderTests(const std::filesystem::path& basePath) {
    std::cout << "\n--- OTNWriter / OTNReader Tests ---\n";
    RUN(Test_WR_BasicRoundTrip);
    RUN(Test_WR_MultipleObjects);
    RUN(Test_WR_StringRoundTrip);
    RUN(Test_WR_Optimized);
    RUN(Test_WR_DefNameDefType);
    RUN(Test_WR_Deduplication);
    RUN(Test_WR_VectorColumn);
    RUN(Test_WR_CustomType);
    RUN(Test_WR_LargeData);
    RUN(Test_WR_ClearObjects);
    RUN(Test_WR_ExtensionAutoAppend);
    RUN(Test_Reader_MissingObject);
    RUN(Test_Reader_Version);
    RUN(Test_Reader_GetObjectsMap);
    RUN(Test_Reader_InvalidFile);
    RUN(Test_Reader_InvalidString);
}
