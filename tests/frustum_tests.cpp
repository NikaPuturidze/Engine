#include "core/camera/camera.h"
#include "graphics/meshes/terrain_mesh.h"
#include "core/player/player.h"
#include "world/block_raycast.h"
#include <stdexcept>

void Require(bool condition)
{
    if (!condition) throw std::runtime_error("Frustum culling regression");
}

class TestGenerator : public WorldGenerator
{
public:
    void Generate(Chunk& chunk) const override
    {
        for (int z = 0; z < Chunk::Depth; ++z)
            for (int x = 0; x < Chunk::Width; ++x)
                chunk.SetBlock(x, 0, z, 1);
    }
};

int main()
{
    Camera camera;
    camera.SetPosition(0, 0, 0);
    const auto frustum = camera.GetFrustum(1.0f, 100.0f);
    Require(frustum.Intersects(DirectX::BoundingBox{{0, 0, 10}, {1, 1, 1}}));
    for (const auto center : {DirectX::XMFLOAT3{0, 0, -10}, {0, 0, 110},
                             {20, 0, 10}, {-20, 0, 10}, {0, 20, 10}, {0, -20, 10}})
        Require(!frustum.Intersects(DirectX::BoundingBox{center, {1, 1, 1}}));
    Require(!frustum.Intersects(DirectX::BoundingBox{{0, 0, 0.04f}, {0.01f, 0.01f, 0.01f}}));
    // Keep boxes crossing a plane, including a box containing the camera.
    Require(frustum.Intersects(DirectX::BoundingBox{{0, 0, 0}, {1, 1, 1}}));
    Require(frustum.Intersects(DirectX::BoundingBox{{7, 0, 10}, {1, 1, 1}}));
    Require(camera.GetFrustum(2.0f).Intersects(DirectX::BoundingBox{{10, 0, 10}, {1, 1, 1}}));
    camera.SetPosition(-30, 10, 20);
    camera.Rotate(90, 0);
    Require(camera.GetFrustum(1.0f).Intersects(DirectX::BoundingBox{{-20, 10, 20}, {1, 1, 1}}));
    Require(!camera.GetFrustum(1.0f).Intersects(DirectX::BoundingBox{{-40, 10, 20}, {1, 1, 1}}));
    camera.Rotate(0, 45);
    Require(camera.GetFrustum(1.0f).Intersects(DirectX::BoundingBox{{-20, 0, 20}, {1, 1, 1}}));

    World::Chunks chunks;
    chunks.emplace(World::ChunkCoordinates{-1, 0}, Chunk(-1, 0));
    chunks.emplace(World::ChunkCoordinates{0, 0}, Chunk(0, 0));
    chunks.at({-1, 0}).SetBlock(15, 0, 0, 1);
    chunks.at({0, 0}).SetBlock(0, 0, 0, 1);
    const auto mesh = BuildTerrainMesh(chunks);
    Require(mesh.chunks.size() == 2 && mesh.blockCount == 2);
    Require(mesh.instances.size() == 10); // Shared boundary faces remain hidden.
    std::size_t instanceCount = 0;
    for (const auto& batch : mesh.batches)
    {
        Require(batch.chunkIndex < mesh.chunks.size());
        const auto& chunk = mesh.chunks[batch.chunkIndex];
        Require(chunk.blockCount == 1);
        Require(batch.firstInstance == instanceCount);
        for (unsigned int i = 0; i < batch.instanceCount; ++i)
            Require(chunk.bounds.Contains(DirectX::BoundingBox{
                mesh.instances[batch.firstInstance + i], {0.5f, 0.5f, 0.5f}}) == DirectX::CONTAINS);
        instanceCount += batch.instanceCount;
    }
    Require(instanceCount == mesh.instances.size());

    World world(std::make_unique<TestGenerator>(), 2);
    world.Update(0, 0, 9);
    Require(world.GetChunks().size() == 9);
    Require(world.GetChunks().contains({0, 0}));
    Require(world.GetChunks().contains({-1, -1}));
    const auto groundHit = RaycastBlock(world, {0.5f, 3.5f, 0.5f}, {0, -1, 0}, 3);
    Require(groundHit && groundHit->block == 1 && groundHit->x == 0 && groundHit->y == 0 && groundHit->z == 0);
    Require(!RaycastBlock(world, {0.5f, 3.5f, 0.5f}, {0, -1, 0}, 2.4f));
    Require(!RaycastBlock(world, {0.5f, 3.5f, 0.5f}, {0, 1, 0}, 10));
    Require(!RaycastBlock(world, {0.5f, 3.5f, 0.5f}, {1, 0, 0}, 10));
    Require(!RaycastBlock(world, {0.5f, 3.5f, 0.5f}, {0, 0, 0}, 10));
    const auto negativeHit = RaycastBlock(world, {-0.5f, 3.5f, -0.5f}, {0, -1, 0}, 3);
    Require(negativeHit && negativeHit->x == -1 && negativeHit->y == 0 && negativeHit->z == -1);
    const auto diagonalHit = RaycastBlock(world, {0.5f, 3.5f, 0.5f}, {-1, -1, -1}, 10);
    Require(diagonalHit && diagonalHit->x == -3 && diagonalHit->y == 0 && diagonalHit->z == -3);
    Player player({0, 100, 0});
    player.Spawn(world);
    Require(player.GetPosition().y == 1.0f);
    player.Update(0.05f, world);
    Require(player.GetPosition().y == 1.0f);
    TerrainMeshCache cache;
    Require(cache.Get(world, player.GetPosition(), 0).empty());
    Require(cache.Get(world, player.GetPosition(), 1).size() == 1);
    Require(cache.Get(world, player.GetPosition(), 1).size() == 2);
    world.Update(0, 0, 1);
    Require(world.GetChunks().size() == 10); // Loading continues while stationary.
    for (int i = 0; i < 30; ++i)
    {
        world.Update(0, 0, 1);
        cache.Get(world, player.GetPosition(), 1);
    }
    Require(world.GetChunks().size() == 25);
    for (int i = 0; i < 30; ++i) cache.Get(world, player.GetPosition(), 1);
    const auto& completed = cache.Get(world, player.GetPosition(), 0);
    Require(completed.size() == 25);
    // Chunk (0,0) now has four neighbors; all side boundary faces were rebuilt away.
    Require(completed.at({0, 0}).mesh.instances.size() == 512);
    const auto revision = world.GetRevision();
    world.Update(0, 0, 1);
    Require(world.GetRevision() == revision);
    world.Update(-16, 0, 1);
    Require(!world.GetChunks().contains({2, 0}));
    for (int i = 0; i < 40; ++i) cache.Get(world, player.GetPosition(), 1);
    Require(!cache.Get(world, player.GetPosition(), 0).contains({2, 0}));
}
