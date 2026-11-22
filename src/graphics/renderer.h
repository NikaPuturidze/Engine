#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <unordered_map>
#include "cube.h"
#include "texture.h"
#include "../core/camera/camera.h"
#include "../core/window.h"

using Microsoft::WRL::ComPtr;


class Renderer
{
	public:
		explicit Renderer(const Window& window);
		~Renderer();

		void BeginFrame(const Camera& camera);
		void Clear(float r, float g, float b) const;
		void EndFrame();
		void DrawCube(const Cube& cube) const;
		void RegisterTexture(const Texture& texture);
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

		ComPtr<ID3D12RootSignature> rootSignature;
		ComPtr<ID3D12PipelineState> pipelineState;
		ComPtr<ID3D12DescriptorHeap> depthHeap;
		ComPtr<ID3D12Resource> depthBuffer;
		UINT width = 0, height = 0;

		ComPtr<ID3D12Resource> vertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

		struct GpuTexture
		{
			ComPtr<ID3D12Resource> resource;
			UINT descriptorIndex = 0;
		};

		static constexpr UINT TextureCapacity = 64;
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
		};

		std::unordered_map<BlockId, BlockTextures> blockTextures;
};
