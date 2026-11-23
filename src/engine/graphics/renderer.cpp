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

Renderer::Renderer(const Window& window)
{
	InitializeGraphicsDevice(window.GetHandle());
	CreateFrameResources();
	CreateSceneResources();
	CreateOverlays();
}

void Renderer::InitializeGraphicsDevice(const HWND window)
{
	CreateAdapter();
	CreateDevice();
	CreateCommandQueue();
	CreateSwapChain(window);
	CreateFenceSynchronization();
}

void Renderer::CreateFrameResources()
{
	CreateDescriptorHeap();
	CreateRenderTargets();
	CreateCommandResources();
}

void Renderer::CreateSceneResources()
{
	CreateVertexBuffer();
	CreateTextureDescriptorHeap();
	CreateDepthBuffer();
	CreatePipeline();
}

void Renderer::CreateOverlays()
{
	debugOverlay = std::make_unique<Overlay>(device.Get());
	overlay = std::make_unique<Overlay>(device.Get());
	targetOverlay = std::make_unique<Overlay>(device.Get());
	crosshairOverlay = std::make_unique<Overlay>(device.Get());
}

void Renderer::DrawOverlay(const std::string_view text) const
{
	overlay->Draw(commandList.Get(), frameIndex, width, height, text);
}

void Renderer::DrawCrosshair(const Texture& image) const
{
	crosshairOverlay->DrawImage(commandList.Get(), frameIndex, width, height, image);
}

void Renderer::DrawTargetOverlay(const std::string_view text) const
{
	targetOverlay->Draw(commandList.Get(), frameIndex, width, height, text, true);
}

Renderer::~Renderer()
{
	if (commandQueue && fence && fenceEvent) WaitForGPU();
	if (fenceEvent) CloseHandle(fenceEvent);
}

void Renderer::BeginFrame(const Camera& camera, const float visibilityDistance)
{
	UpdateCameraViews(camera, visibilityDistance);
	frameIndex = swapChain->GetCurrentBackBufferIndex();
	WaitForCurrentFrame();
	ResetFrameCommands();
	TransitionBackBuffer(D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	BindFrameTarget();
	SetViewport();
}

void Renderer::Clear(const float r, const float g, const float b) const
{
	skyColor = {r, g, b, 1.0f};
	const float color[] = {r, g, b, 1.0f};

	D3D12_CPU_DESCRIPTOR_HANDLE handle =
		rtvHeap->GetCPUDescriptorHandleForHeapStart();

	handle.ptr += frameIndex * rtvDescriptorSize;

	commandList->ClearRenderTargetView(
		handle,
		color,
		0,
		nullptr
	);
}

void Renderer::EndFrame()
{
	TransitionBackBuffer(D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	SubmitFrameCommands();
	PresentFrame();
	SignalFrameFence();
}

void Renderer::TransitionBackBuffer(const D3D12_RESOURCE_STATES before, const D3D12_RESOURCE_STATES after) const
{
	const D3D12_RESOURCE_BARRIER barrier = {
		.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		.Transition = {
			.pResource = renderTargets[frameIndex].Get(),
			.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			.StateBefore = before,
			.StateAfter = after
		}
	};
	commandList->ResourceBarrier(1, &barrier);
}

void Renderer::DrawDebug(const std::vector<DebugLine>& lines) const
{
	debugOverlay->DrawLines(commandList.Get(), frameIndex, width, height, viewProjection, lines);
}

void Renderer::UpdateCameraViews(const Camera& camera, const float visibilityDistance)
{
	cameraPosition = camera.GetPosition();
	this->visibilityDistance = visibilityDistance;
	frustum = camera.GetFrustum(static_cast<float>(width) / height,
	                            100.0f + visibilityDistance * 2.0f);
	DirectX::XMStoreFloat4x4(
		&viewProjection,
		camera.GetViewMatrix() * camera.GetProjectionMatrix(static_cast<float>(width) / height,
		                                                    100.0f + visibilityDistance * 2.0f)
	);
}

void Renderer::WaitForCurrentFrame() const
{
	if (fence->GetCompletedValue() < fenceValues[frameIndex])
	{
		if (FAILED(fence->SetEventOnCompletion(
			fenceValues[frameIndex],
			fenceEvent
		)))
			throw std::runtime_error("Failed to set fence event");

		if (WaitForSingleObject(fenceEvent, INFINITE) != WAIT_OBJECT_0)
			throw std::runtime_error("Failed to wait for GPU");
	}
}

void Renderer::ResetFrameCommands() const
{
	if (FAILED(commandAllocators[frameIndex]->Reset()))
		throw std::runtime_error("Failed to reset allocator");

	if (FAILED(commandList->Reset(
		commandAllocators[frameIndex].Get(),
		nullptr
	)))
		throw std::runtime_error("Failed to reset command list");
}

void Renderer::BindFrameTarget() const
{
	auto rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtv.ptr += frameIndex * rtvDescriptorSize;
	const auto dsv = depthHeap->GetCPUDescriptorHandleForHeapStart();

	commandList->OMSetRenderTargets(
		1,
		&rtv,
		FALSE,
		&dsv
	);

	commandList->ClearDepthStencilView(
		dsv,
		D3D12_CLEAR_FLAG_DEPTH,
		1.0f,
		0,
		0,
		nullptr
	);
}

void Renderer::SetViewport() const
{
	const D3D12_VIEWPORT viewport = {
		.TopLeftX = 0.0f,
		.TopLeftY = 0.0f,
		.Width = static_cast<float>(width),
		.Height = static_cast<float>(height),
		.MinDepth = 0.0f,
		.MaxDepth = 1.0f
	};
	const D3D12_RECT scissor = {
		.left = 0,
		.top = 0,
		.right = static_cast<LONG>(width),
		.bottom = static_cast<LONG>(height)
	};
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissor);
}

void Renderer::SubmitFrameCommands() const
{
	if (FAILED(commandList->Close()))
		throw std::runtime_error("Failed to close command list");

	ID3D12CommandList* lists[] = {commandList.Get()};

	commandQueue->ExecuteCommandLists(1, lists);
}

void Renderer::PresentFrame() const
{
	BOOL fullscreen = FALSE;
	if (tearingSupported && FAILED(swapChain->GetFullscreenState(&fullscreen, nullptr)))
		throw std::runtime_error("Failed to query swap chain fullscreen state");
	const UINT presentFlags = tearingSupported && !fullscreen ? DXGI_PRESENT_ALLOW_TEARING : 0u;
	if (FAILED(swapChain->Present(0, presentFlags)))
		throw std::runtime_error("Failed to present frame");
}

void Renderer::SignalFrameFence()
{
	const UINT64 value = fenceValue++;

	if (FAILED(commandQueue->Signal(fence.Get(), value)))
		throw std::runtime_error("Failed to signal fence");

	fenceValues[frameIndex] = value;
}
