#pragma once

#include <cstdint>

class Cube
{
public:
    explicit Cube(const std::uint64_t id) : id(id) {}

    const std::uint64_t id;
    mutable float x = 0.0f;
    float y = 0.0f;
    float z = 4.0f;
    float size = 1.0f;
};
