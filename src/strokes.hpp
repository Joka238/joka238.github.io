#pragma once
#include <vector>
#include <string>
#include <cstdint>

struct Point { float x; float y; uint64_t t; };

struct Stroke {
    std::string id;
    std::string color;
    int width = 3;
    std::vector<Point> pts;
};

struct FrameData {
    std::string session;
    std::string user;
    uint64_t ts;
    std::vector<Stroke> strokes;
};
