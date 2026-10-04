#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct Vec3 {
    float x{}, y{}, z{};
};

struct Triangle {
    Vec3 n, a, b, c;
};

struct STLModel {
    std::vector<Triangle> triangles;
    Vec3 min{}, max{}, center{};
    std::wstring filename;
    std::wstring format;
    uint64_t fileSize{};
};

bool LoadSTL(const std::wstring& path, STLModel& out, std::wstring& error);
