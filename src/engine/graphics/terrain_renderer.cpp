#include "engine/graphics/renderer.h"
#include "engine/ui/overlay.h"
#include "engine/graphics/meshes/cube_mesh.h"
#include "cube_shader.h"
#include <dxgi1_5.h>
#include <stdexcept>
#include <cstring>
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <queue>

std::size_t Renderer::DrawWorld(const World& world)
{
    const auto& terrainChunks = terrainCache.Get(world, cameraPosition);
    // Only release resources belonging to the frame whose fence BeginFrame waited for.
    auto& frameBuffers = terrainBuffers[frameIndex];
    std::erase_if(frameBuffers, [&](const auto& entry) { return !terrainChunks.contains(entry.first); });
    std::size_t visibleBlockCount = 0;
    std::size_t uploads = 0;
    struct VisibleChunk
    {
        float distanceSquared;
        TerrainMeshCache::Entries::const_iterator chunk;
    };
    const auto farther = [](const VisibleChunk& a, const VisibleChunk& b)
    {
        if (a.distanceSquared != b.distanceSquared)
            return a.distanceSquared > b.distanceSquared;
        return a.chunk->first > b.chunk->first;
    };
    std::priority_queue<VisibleChunk, std::vector<VisibleChunk>, decltype(farther)> visibleChunks(farther);
    for (auto chunk = terrainChunks.begin(); chunk != terrainChunks.end(); ++chunk)
    {
        const auto& mesh = chunk->second.mesh;
        if (mesh.instances.empty()) continue;
        const auto& bounds = mesh.chunks.front().bounds;
        if (!frustum.Intersects(bounds)) continue;
        // Distance to the chunk's footprint puts the chunk containing the player first.
        const float x = std::max(std::abs(cameraPosition.x - bounds.Center.x) - bounds.Extents.x, 0.0f);
        const float z = std::max(std::abs(cameraPosition.z - bounds.Center.z) - bounds.Extents.z, 0.0f);
        visibleChunks.push({.distanceSquared = x * x + z * z, .chunk = chunk});
    }
    // Rebuild priorities every frame so movement and turning immediately change upload order.
    while (!visibleChunks.empty())
    {
        const auto& [coordinates, entry] = *visibleChunks.top().chunk;
        visibleChunks.pop();
        const auto& terrainMesh = entry.mesh;
        const auto terrainGeneration = entry.generation;
        auto& buffer = frameBuffers[coordinates];
        // Defer new or changed chunks once this frame's upload budget is exhausted.
        if (buffer.generation != terrainGeneration && uploads >= 8) continue;
        // BeginFrame has waited for this frame's GPU work before we reuse its upload buffer.
        const auto bytes = terrainMesh.instances.size() * sizeof(DirectX::XMFLOAT3);
        if (buffer.generation != terrainGeneration)
        {
            if (buffer.capacity < bytes)
            {
                constexpr D3D12_HEAP_PROPERTIES heap{.Type = D3D12_HEAP_TYPE_UPLOAD};
                const D3D12_RESOURCE_DESC desc{
                    .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER, .Width = bytes,
                    .Height = 1, .DepthOrArraySize = 1, .MipLevels = 1,
                    .SampleDesc = {.Count = 1}, .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR
                };
                buffer.resource.Reset();
                if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer.resource))))
                    throw std::runtime_error("Failed to create terrain instance buffer");
                buffer.capacity = bytes;
            }
            void* mapped = nullptr;
            constexpr D3D12_RANGE noRead{.Begin = 0, .End = 0};
            if (FAILED(buffer.resource->Map(0, &noRead, &mapped)))
                throw std::runtime_error("Failed to map terrain instances");
            std::memcpy(mapped, terrainMesh.instances.data(), bytes);
            const D3D12_RANGE written{.Begin = 0, .End = bytes};
            buffer.resource->Unmap(0, &written);
            buffer.generation = terrainGeneration;
            ++uploads;
        }
        const D3D12_VERTEX_BUFFER_VIEW views[] = {
            vertexBufferView,
            {.BufferLocation = buffer.resource->GetGPUVirtualAddress(),
             .SizeInBytes = static_cast<UINT>(bytes), .StrideInBytes = sizeof(DirectX::XMFLOAT3)}
        };
        commandList->SetPipelineState(pipelineState.Get());
        commandList->SetGraphicsRootSignature(rootSignature.Get());
        commandList->SetGraphicsRoot32BitConstants(0, 16, &viewProjection, 0);
        const float fogGeometry[] = {-cameraPosition.x, -cameraPosition.z, 0.5f, visibilityDistance};
        commandList->SetGraphicsRoot32BitConstants(0, 4, fogGeometry, 20);
        commandList->SetGraphicsRoot32BitConstants(0, 4, &skyColor, 24);
        const float crackingBlock[] = {crackingPosition.x + 0.5f, crackingPosition.y + 0.5f,
            crackingPosition.z + 0.5f, crackingTexture ? 1.0f : 0.0f};
        commandList->SetGraphicsRoot32BitConstants(0, 4, crackingBlock, 28);
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 2, views);
        ID3D12DescriptorHeap* heaps[] = {textureHeap.Get()};
        commandList->SetDescriptorHeaps(1, heaps);

        for (const auto& batch : terrainMesh.batches)
        {
            const auto textureBinding = blockTextures.find(batch.block);
            if (textureBinding == blockTextures.end())
                throw std::runtime_error("No texture is bound to this block ID");

            const auto sideTexture = textures.find(textureBinding->second.faces[batch.face]);
            const auto topTexture = textures.find(textureBinding->second.top);
            const auto bottomTexture = textures.find(textureBinding->second.bottom);
            if (sideTexture == textures.end() || topTexture == textures.end() || bottomTexture == textures.end())
                throw std::runtime_error("A block texture is not registered");

            const float topTint[] = {
                textureBinding->second.topTintRed,
                textureBinding->second.topTintGreen,
                textureBinding->second.topTintBlue,
                textureBinding->second.sideOverlay != 0 ? 1.0f : 0.0f,
            };
            commandList->SetGraphicsRoot32BitConstants(0, 4, topTint, 16);
            auto textureHandle = [&](const GpuTexture& texture)
            {
                D3D12_GPU_DESCRIPTOR_HANDLE handle = textureHeap->GetGPUDescriptorHandleForHeapStart();
                handle.ptr += texture.descriptorIndex * textureDescriptorSize;
                return handle;
            };
            commandList->SetGraphicsRootDescriptorTable(1, textureHandle(sideTexture->second));
            commandList->SetGraphicsRootDescriptorTable(2, textureHandle(topTexture->second));
            commandList->SetGraphicsRootDescriptorTable(3, textureHandle(bottomTexture->second));
            commandList->SetGraphicsRootDescriptorTable(4,
                textureHandle(crackingTexture ? textures.at(*crackingTexture) : sideTexture->second));
            commandList->SetGraphicsRootDescriptorTable(5, textureHandle(
                textureBinding->second.sideOverlay != 0 ? textures.at(textureBinding->second.sideOverlay) : sideTexture->second));

            commandList->DrawInstanced(6, batch.instanceCount, batch.face * 6, batch.firstInstance);
        }
        visibleBlockCount += terrainMesh.blockCount;
    }
    return visibleBlockCount;
}
