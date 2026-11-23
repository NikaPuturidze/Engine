#pragma once

#include "simulation/world/world.h"
#include "simulation/world/block_faces.h"
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <queue>
#include <set>
#include <optional>

struct TerrainBatch
{
	BlockId block;
	unsigned int face, firstInstance, instanceCount;
	std::size_t chunkIndex;
};

struct TerrainChunk
{
	DirectX::BoundingBox bounds;
	std::size_t blockCount = 0;
};

struct TerrainMesh
{
	std::vector<DirectX::XMFLOAT3> instances;
	std::vector<TerrainBatch> batches;
	std::vector<TerrainChunk> chunks;
	std::size_t blockCount = 0;
};


namespace terrain_detail
{
	class MeshBuilder
	{
		public:
			explicit MeshBuilder(const World::Chunks& chunks) : chunks(chunks)
			{
			}

			TerrainMesh Build(const World::ChunkCoordinates* selectedChunk)
			{
				const auto first = selectedChunk ? chunks.find(*selectedChunk) : chunks.begin();
				const auto last = selectedChunk && first != chunks.end() ? std::next(first) : chunks.end();
				for (auto entry = first; entry != last; ++entry) AppendChunk(entry->first, entry->second);
				return std::move(mesh);
			}

		private:
			using FaceGroups = std::map<std::pair<BlockId, unsigned int>, std::vector<DirectX::XMFLOAT3>>;
			const World::Chunks& chunks;
			TerrainMesh mesh;

			bool IsSolid(const int x, const int y, const int z) const
			{
				if (y < 0 || y >= Chunk::Height) return false;
				const int chunkX = x / Chunk::Width - (x % Chunk::Width < 0 ? 1 : 0);
				const int chunkZ = z / Chunk::Depth - (z % Chunk::Depth < 0 ? 1 : 0);
				const auto chunk = chunks.find({chunkX, chunkZ});
				return chunk != chunks.end() && chunk->second.GetBlock(
					x - chunkX * Chunk::Width, y, z - chunkZ * Chunk::Depth) != Air;
			}

			static DirectX::BoundingBox ChunkBounds(const World::ChunkCoordinates& coordinates, const Chunk& chunk)
			{
				return {
					{
						coordinates.first * Chunk::Width + Chunk::Width * 0.5f,
						chunk.GetStoredHeight() * 0.5f,
						coordinates.second * Chunk::Depth + Chunk::Depth * 0.5f
					},
					{Chunk::Width * 0.5f, chunk.GetStoredHeight() * 0.5f, Chunk::Depth * 0.5f}
				};
			}

			void AppendChunk(const World::ChunkCoordinates& coordinates, const Chunk& chunk)
			{
				FaceGroups groups;
				TerrainChunk terrainChunk{.bounds = ChunkBounds(coordinates, chunk)};
				CollectVisibleFaces(coordinates, chunk, groups, terrainChunk);
				AppendFaceBatches(groups);
				mesh.chunks.push_back(terrainChunk);
			}

			void CollectVisibleFaces(const World::ChunkCoordinates& coordinates, const Chunk& chunk,
			                         FaceGroups& groups, TerrainChunk& terrainChunk)
			{
				for (int y = 0; y < chunk.GetStoredHeight(); ++y)
					for (int z = 0; z < Chunk::Depth; ++z)
						for (int x = 0; x < Chunk::Width; ++x)
						{
							const auto block = chunk.GetBlock(x, y, z);
							if (block == Air) continue;
							const int worldX = coordinates.first * Chunk::Width + x;
							const int worldZ = coordinates.second * Chunk::Depth + z;
							const auto faces = GetVisibleBlockFaces(worldX, y, worldZ,
							                                        [this](const int bx, const int by, const int bz)
							                                        {
								                                        return IsSolid(bx, by, bz);
							                                        });
							if (faces == 0) continue;
							++mesh.blockCount;
							++terrainChunk.blockCount;
							AddBlockFaces(groups, block, faces, {worldX + 0.5f, y + 0.5f, worldZ + 0.5f});
						}
			}

			static void AddBlockFaces(FaceGroups& groups, const BlockId block, const BlockFaceMask faces,
			                          const DirectX::XMFLOAT3 position)
			{
				for (unsigned int face = 0; face < 6; ++face)
					if (faces & (1u << face)) groups[{block, face}].push_back(position);
			}

			void AppendFaceBatches(const FaceGroups& groups)
			{
				for (const auto& [key, positions] : groups)
				{
					mesh.batches.push_back(
						{
							.block = key.first,
							.face = key.second,
							.firstInstance = static_cast<unsigned int>(mesh.instances.size()),
							.instanceCount = static_cast<unsigned int>(positions.size()),
							.chunkIndex = mesh.chunks.size()
						}
					);
					mesh.instances.insert(mesh.instances.end(), positions.begin(), positions.end());
				}
			}
	};
}

inline TerrainMesh BuildTerrainMesh(const World::Chunks& chunks,
                                    const World::ChunkCoordinates* selectedChunk = nullptr)
{
	return terrain_detail::MeshBuilder(chunks).Build(selectedChunk);
}

inline TerrainMesh BuildTerrainMesh(const World& world)
{
	return BuildTerrainMesh(world.GetChunks());
}

class TerrainMeshCache
{
	public:
		struct Entry
		{
			TerrainMesh mesh;
			std::array<std::pair<std::uint64_t, std::uint64_t>, 5> neighbors{};
			std::uint64_t generation = 0;
		};

		using Entries = std::map<World::ChunkCoordinates, Entry>;

		const Entries& Get(const World& world, const DirectX::XMFLOAT3& position, const std::size_t chunkBudget = 8)
		{
			UseWorld(world);
			ReconcileChanges(world, position);
			ScanChunks(world.GetChunks(), position);
			BuildPendingMeshes(world.GetChunks(), chunkBudget);
			return entries;
		}

	private:
		static std::pair<std::uint64_t, std::uint64_t> ChunkVersion(
			const World::Chunks& chunks, const World::ChunkCoordinates& coordinates)
		{
			const auto chunk = chunks.find(coordinates);
			if (chunk == chunks.end()) return {0, 0};
			return {chunk->second.GetLoadVersion(), chunk->second.GetVersion()};
		}

		static std::array<std::pair<std::uint64_t, std::uint64_t>, 5> NeighborMask(
			const World::Chunks& chunks, const World::ChunkCoordinates& coordinates)
		{
			const auto [x, z] = coordinates;
			return {
				ChunkVersion(chunks, {x, z}), ChunkVersion(chunks, {x - 1, z}), ChunkVersion(chunks, {x + 1, z}),
				ChunkVersion(chunks, {x, z - 1}), ChunkVersion(chunks, {x, z + 1})
			};
		}


		void UseWorld(const World& world)
		{
			if (cachedWorld == &world) return;
			entries.clear();
			pending = {};
			queued.clear();
			cachedWorld = &world;
			cachedRevision = world.GetRevision();
			scanning = true;
			scanPosition.reset();
		}

		void QueueChunk(const World::Chunks& chunks, const World::ChunkCoordinates& coordinates,
		                const DirectX::XMFLOAT3& position)
		{
			if (!chunks.contains(coordinates))
			{
				entries.erase(coordinates);
				return;
			}
			if (IsCurrent(chunks, coordinates) || !queued.insert(coordinates).second) return;
			const float x = (coordinates.first + 0.5f) * Chunk::Width - position.x;
			const float z = (coordinates.second + 0.5f) * Chunk::Depth - position.z;
			pending.push({x * x + z * z, coordinates});
		}

		bool IsCurrent(const World::Chunks& chunks, const World::ChunkCoordinates& coordinates) const
		{
			const auto existing = entries.find(coordinates);
			return existing != entries.end() && existing->second.neighbors == NeighborMask(chunks, coordinates);
		}

		void ReconcileChanges(const World& world, const DirectX::XMFLOAT3& position)
		{
			if (cachedRevision == world.GetRevision()) return;
			const auto& chunks = world.GetChunks();
			if (cachedRevision + 1 != world.GetRevision())
			{
				scanning = true;
				scanPosition.reset();
				std::erase_if(entries, [&](const auto& entry) { return !chunks.contains(entry.first); });
			}
			for (const auto coordinates : world.GetChangedChunks()) InvalidateNeighborhood(
				chunks, coordinates, position);
			cachedRevision = world.GetRevision();
		}

		void InvalidateNeighborhood(const World::Chunks& chunks, const World::ChunkCoordinates& coordinates,
		                            const DirectX::XMFLOAT3& position)
		{
			const auto [x, z] = coordinates;
			const World::ChunkCoordinates affected[] = {coordinates, {x - 1, z}, {x + 1, z}, {x, z - 1}, {x, z + 1}};
			// Remove all affected meshes before queueing any of them, including neighbors.
			for (const auto affectedChunk : affected) entries.erase(affectedChunk);
			for (const auto affectedChunk : affected) QueueChunk(chunks, affectedChunk, position);
		}

		void ScanChunks(const World::Chunks& chunks, const DirectX::XMFLOAT3& position)
		{
			if (!scanning) return;
			auto next = scanPosition ? chunks.upper_bound(*scanPosition) : chunks.begin();
			for (unsigned int i = 0; next != chunks.end() && i < 128; ++i, ++next)
			{
				QueueChunk(chunks, next->first, position);
				scanPosition = next->first;
			}
			scanning = next != chunks.end();
		}

		void BuildPendingMeshes(const World::Chunks& chunks, const std::size_t chunkBudget)
		{
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2);
			std::size_t built = 0, examined = 0;
			while (!pending.empty() && built < chunkBudget && examined++ < 128)
			{
				const auto coordinates = pending.top().second;
				pending.pop();
				queued.erase(coordinates);
				if (!chunks.contains(coordinates) || IsCurrent(chunks, coordinates)) continue;
				entries[coordinates] = {
					.mesh = BuildTerrainMesh(chunks, &coordinates),
					.neighbors = NeighborMask(chunks, coordinates),
					.generation = ++generation
				};
				++built;
				if (std::chrono::steady_clock::now() >= deadline) break;
			}
		}


		using PendingChunk = std::pair<float, World::ChunkCoordinates>;
		std::priority_queue<PendingChunk, std::vector<PendingChunk>, std::greater<PendingChunk>> pending;
		std::set<World::ChunkCoordinates> queued;
		std::optional<World::ChunkCoordinates> scanPosition;
		bool scanning = false;
		std::uint64_t cachedRevision = 0;
		Entries entries;
		const World* cachedWorld = nullptr;
		std::uint64_t generation = 0;
};
