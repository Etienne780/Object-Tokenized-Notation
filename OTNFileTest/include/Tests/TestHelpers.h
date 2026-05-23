#pragma once
#include <filesystem>
#include <string>
#include <OTNFile/OTNFile.h>

// ============================================================
//  Path helper
// ============================================================

inline std::filesystem::path MakeTestFile(const std::filesystem::path& dir,
                                           const std::string& name) {
    return dir / name;
}

// ============================================================
//  Custom type used across tests
// ============================================================

struct Vec2 { 
    float x = 0.0f;
    float y = 0.0f; 
};

template<>
inline void OTN::ToOTNDataType<Vec2>(OTN::OTNObjectBuilder& b, Vec2& v) {
    b.SetObjectName("Vec2");
    b.AddNames("x", "y");
    b.AddData(v.x, v.y);
}
