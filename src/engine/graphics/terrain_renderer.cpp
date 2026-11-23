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


bool Renderer::FartherChunk::operator()(const VisibleChunk& left, const VisibleChunk& right) const
{
	if (left.distanceSquared != right.distanceSquared) return left.distanceSquared > right.distanceSquared;
	return left.chunk->first > right.chunk->first;
}

std::size_t Renderer::DrawWorld(const World& world)
{
	const auto& chunks = terrainCache.Get(world, cameraPosition);
	ReleaseUnusedTerrainBuffers(chunks);
	auto visible = CollectVisibleChunks(chunks);
	return DrawVisibleChunks(visible);
}

void Renderer::ReleaseUnusedTerrainBuffers(const TerrainMeshCache::Entries& chunks)
{
	// BeginFrame has waited for this frame's fence before resources are released.
	std::erase_if(terrainBuffers[frameIndex], [&](const auto& entry) { return !chunks.contains(entry.first); });
}

Renderer::VisibleChunks Renderer::CollectVisibleChunks(const TerrainMeshCache::Entries& chunks) const
{
	VisibleChunks visible;
	for (auto chunk = chunks.begin(); chunk != chunks.end(); ++chunk)
	{
		const auto& mesh = chunk->second.mesh;
		if (mesh.instances.empty()) continue;
		const auto& bounds = mesh.chunks.front().bounds;
		if (!frustum.Intersects(bounds)) continue;
		const float x = std::max(std::abs(cameraPosition.x - bounds.Center.x) - bounds.Extents.x, 0.0f);
		const float z = std::max(std::abs(cameraPosition.z - bounds.Center.z) - bounds.Extents.z, 0.0f);
		visible.push({.distanceSquared = x * x + z * z, .chunk = chunk});
	}
	return visible;
}

std::size_t Renderer::DrawVisibleChunks(VisibleChunks& visible)
{
	std::size_t blockCount = 0, uploads = 0;
	while (!visible.empty())
	{
		const auto& [coordinates, entry] = *visible.top().chunk;
		visible.pop();
		auto& buffer = terrainBuffers[frameIndex][coordinates];
		if (!PrepareTerrainBuffer(entry, buffer, uploads)) continue;
		BindTerrainBuffer(entry.mesh, buffer);
		BindTerrainPipeline();
		for (const auto& batch : entry.mesh.batches) DrawTerrainBatch(batch);
		blockCount += entry.mesh.blockCount;
	}
	return blockCount;
}

bool Renderer::PrepareTerrainBuffer(const TerrainMeshCache::Entry& entry, TerrainBuffer& buffer,
                                    std::size_t& uploads) const
{
	if (buffer.generation == entry.generation) return true;
	if (uploads >= 8) return false;
	UploadTerrainMesh(entry, buffer);
	++uploads;
	return true;
}

void Renderer::UploadTerrainMesh(const TerrainMeshCache::Entry& entry, TerrainBuffer& buffer) const
{
	const auto bytes = entry.mesh.instances.size() * sizeof(DirectX::XMFLOAT3);
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
	std::memcpy(mapped, entry.mesh.instances.data(), bytes);
	const D3D12_RANGE written{.Begin = 0, .End = bytes};
	buffer.resource->Unmap(0, &written);
	buffer.generation = entry.generation;
}

void Renderer::BindTerrainBuffer(const TerrainMesh& terrainMesh, const TerrainBuffer& buffer) const
{
	const auto bytes = terrainMesh.instances.size() * sizeof(DirectX::XMFLOAT3);
	const D3D12_VERTEX_BUFFER_VIEW views[] = {
		vertexBufferView,
		{
			.BufferLocation = buffer.resource->GetGPUVirtualAddress(),
			.SizeInBytes = static_cast<UINT>(bytes), .StrideInBytes = sizeof(DirectX::XMFLOAT3)
		}
	};

	commandList->IASetVertexBuffers(0, 2, views);
}

void Renderer::BindTerrainPipeline() const
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetGraphicsRootSignature(rootSignature.Get());
	commandList->SetGraphicsRoot32BitConstants(0, 16, &viewProjection, 0);
	const float fogGeometry[] = {-cameraPosition.x, -cameraPosition.z, 0.5f, visibilityDistance};
	commandList->SetGraphicsRoot32BitConstants(0, 4, fogGeometry, 20);
	commandList->SetGraphicsRoot32BitConstants(0, 4, &skyColor, 24);
	const float crackingBlock[] = {
		crackingPosition.x + 0.5f, crackingPosition.y + 0.5f,
		crackingPosition.z + 0.5f, crackingTexture ? 1.0f : 0.0f
	};
	commandList->SetGraphicsRoot32BitConstants(0, 4, crackingBlock, 28);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D12DescriptorHeap* heaps[] = {textureHeap.Get()};
	commandList->SetDescriptorHeaps(1, heaps);
}

D3D12_GPU_DESCRIPTOR_HANDLE Renderer::TextureHandle(const GpuTexture& texture) const
{
	auto handle = textureHeap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += texture.descriptorIndex * textureDescriptorSize;
	return handle;
}

void Renderer::DrawTerrainBatch(const TerrainBatch& batch) const
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
	commandList->SetGraphicsRootDescriptorTable(1, TextureHandle(sideTexture->second));
	commandList->SetGraphicsRootDescriptorTable(2, TextureHandle(topTexture->second));
	commandList->SetGraphicsRootDescriptorTable(3, TextureHandle(bottomTexture->second));
	commandList->SetGraphicsRootDescriptorTable(4,
	                                            TextureHandle(
		                                            crackingTexture
			                                            ? textures.at(*crackingTexture)
			                                            : sideTexture->second));
	commandList->SetGraphicsRootDescriptorTable(5, TextureHandle(
		                                            textureBinding->second.sideOverlay != 0
			                                            ? textures.at(textureBinding->second.sideOverlay)
			                                            : sideTexture->second));

	commandList->DrawInstanced(6, batch.instanceCount, batch.face * 6, batch.firstInstance);
}
