#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

#include "cube.h"
#include "../core/camera/camera.h"

class Renderer
{
public:
	explicit Renderer(HWND hwnd);
	~Renderer();

	void BeginFrame(const Camera& camera);
	void Clear(float r, float g, float b) const;
	void EndFrame();
	void DrawCube(const Cube& cube) const;
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
};
