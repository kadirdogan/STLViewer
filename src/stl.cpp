#include "stl.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <limits>
#include <filesystem>
#include <cstring>

static void bounds(STLModel& m) {
    const float inf = std::numeric_limits<float>::infinity();
    m.min = { inf, inf, inf };
    m.max = {-inf,-inf,-inf };
    auto add = [&](const Vec3& v) {
        m.min.x = std::min(m.min.x,v.x); m.min.y = std::min(m.min.y,v.y); m.min.z = std::min(m.min.z,v.z);
        m.max.x = std::max(m.max.x,v.x); m.max.y = std::max(m.max.y,v.y); m.max.z = std::max(m.max.z,v.z);
    };
    for (auto& t:m.triangles) { add(t.a); add(t.b); add(t.c); }
    m.center = {(m.min.x+m.max.x)/2.f,(m.min.y+m.max.y)/2.f,(m.min.z+m.max.z)/2.f};
}

static bool loadBinary(std::ifstream& f, uint64_t size, STLModel& m) {
    char header[80]; uint32_t count=0;
    f.read(header,80); f.read(reinterpret_cast<char*>(&count),4);
    if (!f || 84ull + 50ull*count != size) return false;
    m.triangles.resize(count);
    for(uint32_t i=0;i<count;i++) {
        float v[12]; uint16_t attr;
        f.read(reinterpret_cast<char*>(v),48);
        f.read(reinterpret_cast<char*>(&attr),2);
        if(!f) return false;
        m.triangles[i]={{v[0],v[1],v[2]},{v[3],v[4],v[5]},{v[6],v[7],v[8]},{v[9],v[10],v[11]}};
    }
    m.format=L"Binary STL";
    return true;
}

static bool loadAscii(std::ifstream& f, STLModel& m) {
    f.clear(); f.seekg(0);
    std::string word;
    Triangle t{};
    int vi=0;
    while(f>>word) {
        if(word=="facet") {
            f>>word; // normal
            f>>t.n.x>>t.n.y>>t.n.z;
        } else if(word=="vertex") {
            Vec3 v; f>>v.x>>v.y>>v.z;
            if(vi==0)t.a=v; else if(vi==1)t.b=v; else t.c=v;
            vi++;
            if(vi==3){m.triangles.push_back(t); vi=0;}
        }
    }
    if(m.triangles.empty()) return false;
    m.format=L"ASCII STL";
    return true;
}

bool LoadSTL(const std::wstring& path, STLModel& out, std::wstring& error) {
    try {
        std::ifstream f(std::filesystem::path(path), std::ios::binary);
        if(!f){error=L"Could not open file."; return false;}
        auto size=std::filesystem::file_size(path);
        STLModel m; m.filename=std::filesystem::path(path).filename().wstring(); m.fileSize=size;
        bool ok=loadBinary(f,size,m);
        if(!ok){m.triangles.clear(); ok=loadAscii(f,m);}
        if(!ok){error=L"File is not a valid binary or ASCII STL."; return false;}
        bounds(m); out=std::move(m); return true;
    } catch(const std::exception&) {
        error=L"Unexpected error while reading STL."; return false;
    }
}
