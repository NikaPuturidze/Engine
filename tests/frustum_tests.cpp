#include "core/camera/camera.h"
#include "graphics/meshes/terrain_mesh.h"
#include "core/player/player.h"
#include "core/player/sprint_input.h"
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
    SprintInput sprintInput;
    const SprintInput::Clock::time_point start{};
    using namespace std::chrono_literals;
    sprintInput.SetForward(true, start);
    sprintInput.SetForward(true, start + 50ms);
    Require(!sprintInput.IsSprinting()); // Holding W cannot activate sprint.
    sprintInput.SetForward(false, start + 100ms);
    sprintInput.SetForward(true, start + 200ms);
    Require(sprintInput.IsSprinting());
    sprintInput.SetForward(true, start + 250ms);
    Require(sprintInput.IsSprinting());
    sprintInput.SetForward(false, start + 300ms);
    Require(!sprintInput.IsSprinting());
    sprintInput.SetForward(true, start + 400ms);
    sprintInput.SetForward(false, start + 450ms);
    sprintInput.SetForward(true, start + 800ms);
    Require(!sprintInput.IsSprinting()); // Slow taps remain walking.
    sprintInput.Reset();
    sprintInput.SetForward(true, start + 850ms);
    Require(!sprintInput.IsSprinting()); // Focus loss clears the tap window.
    sprintInput.SetForward(false, start + 900ms);
    sprintInput.SetForward(true, start + 950ms);
    Require(sprintInput.IsSprinting());
    sprintInput.Cancel();
    Require(!sprintInput.IsSprinting());

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
    Player runner({0.5f, 100, 0.5f});
    runner.Spawn(world);
    for (int i = 0; i < 60; ++i)
    {
        runner.Move(0, 1, 0, 0.05f, true);
        runner.Update(0.05f, world);
    }
    const float beforeSprintTick = runner.GetPosition().z;
    runner.Move(0, 1, 0, 0.05f, true);
    runner.Update(0.05f, world);
    Require(std::abs((runner.GetPosition().z - beforeSprintTick) * 20 - 5.612335f) < 0.001f);

    Player sprintJumper({0.5f, 100, 0.5f});
    sprintJumper.Spawn(world);
    sprintJumper.Move(0, 1, 90, 0.05f, true);
    sprintJumper.Jump();
    sprintJumper.Update(0.05f, world);
    Require(std::abs(sprintJumper.GetPosition().x - 0.8274f) < 0.00001f);
    Require(std::abs(sprintJumper.GetPosition().z - 0.5f) < 0.00001f);
    sprintJumper.Jump();
    sprintJumper.Update(0.05f, world);
    Require(std::abs(sprintJumper.GetPosition().x - (0.8274f + 0.3274f * 0.91f)) < 0.00001f);

    Player backwardJumper({0.5f, 100, 0.5f});
    backwardJumper.Spawn(world);
    backwardJumper.Move(0, -1, 0, 0.05f, true);
    backwardJumper.Jump();
    backwardJumper.Update(0.05f, world);
    Require(std::abs(backwardJumper.GetPosition().z - 0.402f) < 0.00001f);
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
    player.Jump();
    player.Update(0.05f, world);
    Require(std::abs(player.GetPosition().y - 1.42f) < 0.00001f);
    float jumpPeak = player.GetPosition().y;
    int jumpTicks = 1;
    while (player.GetPosition().y > 1.0f && jumpTicks < 30)
    {
        player.Jump(); // Airborne jump requests cannot reset upward velocity.
        player.Update(0.05f, world);
        jumpPeak = std::max(jumpPeak, player.GetPosition().y);
        ++jumpTicks;
    }
    Require(jumpTicks == 12);
    Require(std::abs(jumpPeak - 2.252203f) < 0.00001f);
    Require(player.GetPosition().y == 1.0f);
    player.Jump(); // Landing enables the next jump.
    player.Update(0.05f, world);
    Require(std::abs(player.GetPosition().y - 1.42f) < 0.00001f);
    for (int i = 0; i < 12; ++i) player.Update(0.05f, world);
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
    // Removing a boundary block also reveals the adjacent chunk's hidden face.
    const auto neighborFaces = completed.at({-1, 0}).mesh.instances.size();
    Require(world.SetBlock(0, 0, 0, 0));
    Require(!world.SetBlock(0, 0, 0, 0));
    Require(!world.SetBlock(0, -1, 0, 0));
    Require(!world.SetBlock(10000, 0, 0, 0));
    Require(!RaycastBlock(world, {0.5f, 3.5f, 0.5f}, {0, -1, 0}, 10));
    for (int i = 0; i < 30; ++i) cache.Get(world, player.GetPosition(), 1);
    Require(cache.Get(world, player.GetPosition(), 0).at({-1, 0}).mesh.instances.size() == neighborFaces + 1);
    // Multiple mutations between renders must invalidate older cached contents too.
    Require(world.SetBlock(-1, 0, -1, 0));
    Require(world.SetBlock(1, 0, 1, 0));
    for (int i = 0; i < 60; ++i) cache.Get(world, player.GetPosition(), 1);
    Require(cache.Get(world, player.GetPosition(), 0).at({0, 0}).mesh.blockCount == 254);
    world.Update(-16, 0, 1);
    Require(!world.GetChunks().contains({2, 0}));
    for (int i = 0; i < 40; ++i) cache.Get(world, player.GetPosition(), 1);
    Require(!cache.Get(world, player.GetPosition(), 0).contains({2, 0}));
    world.Update(160, 0, 1);
    Require(!world.GetChunks().contains({0, 0}));
    world.Update(0, 0, 1);
    Require(world.GetChunks().contains({0, 0}));
    Require(world.GetBlock(0, 0, 0) == 0);
    Require(world.GetBlock(1, 0, 1) == 0);
}
