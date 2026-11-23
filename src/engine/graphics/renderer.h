#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <unordered_map>
#include <memory>
#include <string_view>
#include "api/types.h"
#include "api/game.h"
#include "simulation/world/block_faces.h"
#include "engine/graphics/texture.h"
#include "engine/graphics/meshes/terrain_mesh.h"
#include "engine/graphics/camera/camera.h"
#include "engine/platform/window.h"

using Microsoft::WRL::ComPtr;

class Overlay;

class Renderer
{
	public:
		explicit Renderer(const Window& window);
		~Renderer();

		void BeginFrame(const Camera& camera, float visibilityDistance);
		void Clear(float r, float g, float b) const;
		void EndFrame();
		void DrawDebug(const std::vector<DebugLine>& lines) const;
		void DrawOverlay(std::string_view text) const;
		void DrawTargetOverlay(std::string_view text) const;
		void DrawCrosshair(const Texture& image) const;
		std::size_t DrawWorld(const World& world);
		void RegisterTexture(const Texture& texture);
		void BindFaceTextures(BlockId blockId, const std::array<TextureId, 6>& faces);
		void BindSideOverlay(BlockId blockId, TextureId texture);
		void SetBlockCracking(BlockPosition position, std::optional<TextureId> texture);
		void BindTextures(
			BlockId blockId,
			TextureId sideTexture,
			TextureId topTexture,
			TextureId bottomTexture,
			float topTintRed,
			float topTintGreen,
			float topTintBlue
		);

	private:
		void CreateAdapter();
		void CreateDevice();
		void CreateCommandQueue();
		void CreateSwapChain(HWND hwnd);
		void CreateFenceSynchronization();
		void WaitForGPU();
		void CreateDescriptorHeap();
		void CreateRenderTargets();
		void CreateCommandResources();
		void CreateVertexBuffer();
		void CreateTextureDescriptorHeap();
		void CreatePipeline();
		void CreateDepthBuffer();

		ComPtr<IDXGIFactory4> factory;
		ComPtr<IDXGIAdapter1> adapter;
		ComPtr<ID3D12Device> device;
		ComPtr<ID3D12CommandQueue> commandQueue;
		ComPtr<IDXGISwapChain3> swapChain;
		bool tearingSupported = false;

		ComPtr<ID3D12Fence> fence;
		static constexpr UINT BufferCount = 2;
		HANDLE fenceEvent = nullptr;
		UINT64 fenceValue = 1;
		UINT64 fenceValues[2] = {};

		ComPtr<ID3D12DescriptorHeap> rtvHeap;
		UINT rtvDescriptorSize = 0;

		ComPtr<ID3D12Resource> renderTargets[BufferCount];
		ComPtr<ID3D12CommandAllocator> commandAllocators[BufferCount];
		ComPtr<ID3D12GraphicsCommandList> commandList;
		UINT frameIndex = 0;
		DirectX::XMFLOAT4X4 viewProjection{};
		DirectX::BoundingFrustum frustum;
		DirectX::XMFLOAT3 cameraPosition{};
		float visibilityDistance = 0.0f;
		mutable DirectX::XMFLOAT4 skyColor{0.3f, 0.6f, 0.9f, 1.0f};

		ComPtr<ID3D12RootSignature> rootSignature;
		ComPtr<ID3D12PipelineState> pipelineState;
		ComPtr<ID3D12DescriptorHeap> depthHeap;
		ComPtr<ID3D12Resource> depthBuffer;
		UINT width = 0, height = 0;

		ComPtr<ID3D12Resource> vertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
		TerrainMeshCache terrainCache;
		struct TerrainBuffer
		{
			ComPtr<ID3D12Resource> resource;
			std::size_t capacity = 0;
			std::uint64_t generation = 0;
		};
		std::map<World::ChunkCoordinates, TerrainBuffer> terrainBuffers[BufferCount];

		struct GpuTexture
		{
			ComPtr<ID3D12Resource> resource;
			UINT descriptorIndex = 0;
		};

		static constexpr UINT TextureCapacity = 8192;
		ComPtr<ID3D12DescriptorHeap> textureHeap;
		UINT textureDescriptorSize = 0;
		std::unordered_map<TextureId, GpuTexture> textures;

		struct BlockTextures
		{
			TextureId side;
			TextureId top;
			TextureId bottom;
			float topTintRed;
			float topTintGreen;
			float topTintBlue;
			std::array<TextureId, 6> faces;
			TextureId sideOverlay = 0;
		};

		std::unordered_map<BlockId, BlockTextures> blockTextures;
		BlockPosition crackingPosition{};
		std::optional<TextureId> crackingTexture;
		std::unique_ptr<Overlay> debugOverlay;
		std::unique_ptr<Overlay> overlay;
		std::unique_ptr<Overlay> targetOverlay;
		std::unique_ptr<Overlay> crosshairOverlay;
};
