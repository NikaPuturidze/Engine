#include "../src/core/entity/entity.h"
#include "../src/world/world.h"
#include <cmath>
#include <stdexcept>
#include <iostream>

class CollisionWorld final : public WorldGenerator
{
public:
    void Generate(Chunk& chunk) const override
    {
        for (int z = 0; z < Chunk::Depth; ++z)
            for (int x = 0; x < Chunk::Width; ++x)
            {
                const int worldX = chunk.GetChunkX() * Chunk::Width + x;
                if (worldX != 6) chunk.SetBlock(x, 0, z, 1);
                if (worldX == 3 || worldX == -3)
                    for (int y = 1; y < Chunk::Height; ++y)
                        chunk.SetBlock(x, y, z, 2);
                if (worldX == 6) chunk.SetBlock(x, 2, z, 3);
            }
    }
};

class TestEntity final : public Entity
{
public:
    using Entity::Entity;
    void Update(float dt, const World& world) override { UpdatePhysics(dt, world); }
    void Sweep(int axis, float distance, const World& world) { MoveAlongAxis(axis, distance, world); }
    void SetVelocity(float x, float y, float z) { velocity = {x, y, z}; }
    DirectX::XMFLOAT3 GetVelocity() const { return velocity; }
};

void Check(float actual, float expected)
{
    if (std::abs(actual - expected) > 0.0001f)
        throw std::runtime_error("Expected " + std::to_string(expected) + ", got " + std::to_string(actual));
}

int main()
{
    World world(std::make_unique<CollisionWorld>(), 1);
    world.Update(0, 0);

    TestEntity falling({0, 100, 0});
    for (int i = 0; i < 400; ++i) falling.Update(0.05f, world);
    Check(falling.GetPosition().y, 0.5f);
    Check(falling.GetVelocity().y, 0);

    TestEntity wall({0, 0.5f, 0});
    wall.SetVelocity(100, 0, 2);
    wall.Update(0.05f, world);
    Check(wall.GetPosition().x, 2.2f);
    Check(wall.GetVelocity().x, 0);
    Check(wall.GetPosition().z, 0.1f);
    for (int i = 0; i < 100; ++i) wall.Sweep(0, 0.1f, world);
    Check(wall.GetPosition().x, 2.2f);

    TestEntity negative({0, 0.5f, -1});
    negative.Sweep(0, -10, world);
    Check(negative.GetPosition().x, -2.2f);

    TestEntity ceiling({6, -0.4f, 0});
    ceiling.SetVelocity(0, 100, 0);
    ceiling.Update(0.05f, world);
    Check(ceiling.GetPosition().y, -0.3f);
    Check(ceiling.GetVelocity().y, 0);

    TestEntity air({0, 5, 0});
    air.Sweep(0, 10, world);
    Check(air.GetPosition().x, 10);

    TestEntity seam({0, 10, -0.5f});
    seam.Sweep(1, -20, world);
    Check(seam.GetPosition().y, 0.5f);
    seam.Sweep(2, -17, world);
    Check(seam.GetPosition().z, -17.5f);

    std::cout << "Collision checks passed\n";
}
