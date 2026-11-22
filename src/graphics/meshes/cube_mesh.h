#pragma once

struct Vertex
{
    float x, y, z;
	float u, v;
	unsigned int faceType;
};

inline constexpr Vertex cubeVertices[] = {
    // Front
    {.x = -1, .y = -1, .z = -1, .u = 0, .v = 1, .faceType = 0}, {.x = -1, .y = 1, .z = -1, .u = 0, .v = 0, .faceType = 0}, {.x = 1, .y = 1, .z = -1, .u = 1, .v = 0, .faceType = 0},
    {.x = -1, .y = -1, .z = -1, .u = 0, .v = 1, .faceType = 0}, {.x = 1, .y = 1, .z = -1, .u = 1, .v = 0, .faceType = 0}, {.x = 1, .y = -1, .z = -1, .u = 1, .v = 1, .faceType = 0},
    // Back
    {.x = -1, .y = -1, .z = 1, .u = 0, .v = 1, .faceType = 1}, {.x = 1, .y = -1, .z = 1, .u = 1, .v = 1, .faceType = 1}, {.x = 1, .y = 1, .z = 1, .u = 1, .v = 0, .faceType = 1},
    {.x = -1, .y = -1, .z = 1, .u = 0, .v = 1, .faceType = 1}, {.x = 1, .y = 1, .z = 1, .u = 1, .v = 0, .faceType = 1}, {.x = -1, .y = 1, .z = 1, .u = 0, .v = 0, .faceType = 1},
    // Left
    {.x = -1, .y = -1, .z = 1, .u = 0, .v = 1, .faceType = 2}, {.x = -1, .y = 1, .z = 1, .u = 0, .v = 0, .faceType = 2}, {.x = -1, .y = 1, .z = -1, .u = 1, .v = 0, .faceType = 2},
    {.x = -1, .y = -1, .z = 1, .u = 0, .v = 1, .faceType = 2}, {.x = -1, .y = 1, .z = -1, .u = 1, .v = 0, .faceType = 2}, {.x = -1, .y = -1, .z = -1, .u = 1, .v = 1, .faceType = 2},
    // Right
    {.x = 1, .y = -1, .z = -1, .u = 0, .v = 1, .faceType = 3}, {.x = 1, .y = 1, .z = -1, .u = 0, .v = 0, .faceType = 3}, {.x = 1, .y = 1, .z = 1, .u = 1, .v = 0, .faceType = 3},
    {.x = 1, .y = -1, .z = -1, .u = 0, .v = 1, .faceType = 3}, {.x = 1, .y = 1, .z = 1, .u = 1, .v = 0, .faceType = 3}, {.x = 1, .y = -1, .z = 1, .u = 1, .v = 1, .faceType = 3},
    // Top
    {.x = -1, .y = 1, .z = -1, .u = 0, .v = 1, .faceType = 4}, {.x = -1, .y = 1, .z = 1, .u = 0, .v = 0, .faceType = 4}, {.x = 1, .y = 1, .z = 1, .u = 1, .v = 0, .faceType = 4},
    {.x = -1, .y = 1, .z = -1, .u = 0, .v = 1, .faceType = 4}, {.x = 1, .y = 1, .z = 1, .u = 1, .v = 0, .faceType = 4}, {.x = 1, .y = 1, .z = -1, .u = 1, .v = 1, .faceType = 4},
    // Bottom
    {.x = -1, .y = -1, .z = 1, .u = 0, .v = 1, .faceType = 5}, {.x = -1, .y = -1, .z = -1, .u = 0, .v = 0, .faceType = 5}, {.x = 1, .y = -1, .z = -1, .u = 1, .v = 0, .faceType = 5},
    {.x = -1, .y = -1, .z = 1, .u = 0, .v = 1, .faceType = 5}, {.x = 1, .y = -1, .z = -1, .u = 1, .v = 0, .faceType = 5}, {.x = 1, .y = -1, .z = 1, .u = 1, .v = 1, .faceType = 5},
};
