#include "renderer.h"
#include "../ui/overlay.h"
#include "meshes/cube_mesh.h"
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
	CreateAdapter();
	CreateDevice();
	CreateCommandQueue();
	CreateSwapChain(window.GetHandle());
	CreateFenceSynchronization();
	CreateDescriptorHeap();
	CreateRenderTargets();
	CreateCommandResources();
	CreateVertexBuffer();
	CreateTextureDescriptorHeap();
	CreateDepthBuffer();
	CreatePipeline();
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

void Renderer::CreateAdapter()
{
	if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
	{
		throw std::runtime_error("Failed to create DXGIFactory");
	}

	if (FAILED(factory->EnumAdapters1(0, &adapter)))
	{
		throw std::runtime_error("Failed to create IDXGIAdapter1");
	}
}

void Renderer::CreateDevice()
{
	if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
	{
		throw std::runtime_error("Failed to create D3D12 device");
	}
}

void Renderer::CreateCommandQueue()
{
	constexpr D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
	};

	if (FAILED(device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue))))
	{
		throw std::runtime_error("Failed to create command queue");
	}
}

void Renderer::CreateSwapChain(const HWND hwnd)
{
	ComPtr<IDXGIFactory5> factory5;
	BOOL allowTearing = FALSE;
	tearingSupported = SUCCEEDED(factory.As(&factory5)) &&
		SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
			&allowTearing, sizeof(allowTearing))) && allowTearing;
	const DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
		.Width = 0,
		.Height = 0,
		.Format = DXGI_FORMAT_B8G8R8A8_UNORM,
		.SampleDesc = {
			.Count = 1
		},
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = 2,
		.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
		.Flags = tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u,
	};

	ComPtr<IDXGISwapChain1> swapChain1;

	const HRESULT result = factory->CreateSwapChainForHwnd(
		commandQueue.Get(),
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain1
	);

	if (FAILED(result))
		throw std::runtime_error("Failed to create swap chain: " + std::to_string(result));

	if (FAILED(swapChain1.As(&swapChain)))
		throw std::runtime_error("Failed to get IDXGISwapChain3");
}

void Renderer::CreateFenceSynchronization()
{
	if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
	{
		throw std::runtime_error("Failed to create fence");
	}

	fenceEvent = CreateEvent(
		nullptr,
		FALSE,
		FALSE,
		nullptr
	);

	if (!fenceEvent)
		throw std::runtime_error("Failed to create fence event");
}

void Renderer::WaitForGPU()
{
	const UINT64 value = fenceValue++;

	if (FAILED(commandQueue->Signal(fence.Get(), value)))
	{
		throw std::runtime_error("Failed to signal fence");
	}

	if (fence->GetCompletedValue() < value)
	{
		if (FAILED(fence->SetEventOnCompletion(value,fenceEvent)))
			throw std::runtime_error("Failed to set fence event");

		if (WaitForSingleObject(fenceEvent, INFINITE) != WAIT_OBJECT_0)
			throw std::runtime_error("Failed to wait for GPU");
	}
}

void Renderer::CreateDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		.NumDescriptors = BufferCount,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	};

	if (FAILED(device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&rtvHeap))))
	{
		throw std::runtime_error("Failed to create RTV descriptor heap");
	}

	rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV
	);
}

void Renderer::CreateRenderTargets()
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle =
		rtvHeap->GetCPUDescriptorHandleForHeapStart();

	for (UINT i = 0; i < BufferCount; i++)
	{
		if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]))))
		{
			throw std::runtime_error("Failed to get swap chain buffer");
		}

		device->CreateRenderTargetView(
			renderTargets[i].Get(),
			nullptr,
			handle
		);

		handle.ptr += rtvDescriptorSize;
	}
}

void Renderer::CreateCommandResources()
{
	for (UINT i = 0; i < BufferCount; i++)
	{
		HRESULT result = device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&commandAllocators[i])
		);

		if (FAILED(result))
			throw std::runtime_error("Failed to create command allocator");
	}

	HRESULT result = device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		commandAllocators[0].Get(),
		nullptr,
		IID_PPV_ARGS(&commandList)
	);

	if (FAILED(result))
		throw std::runtime_error("Failed to create command list");

	result = commandList->Close();

	if (FAILED(result))
		throw std::runtime_error("Failed to close command list");
}

void Renderer::BeginFrame(const Camera& camera, const float visibilityDistance)
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
	frameIndex = swapChain->GetCurrentBackBufferIndex();

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

	if (FAILED(commandAllocators[frameIndex]->Reset()))
		throw std::runtime_error("Failed to reset allocator");

	if (FAILED(commandList->Reset(
		commandAllocators[frameIndex].Get(),
		nullptr
	)))
		throw std::runtime_error("Failed to reset command list");

	const D3D12_RESOURCE_BARRIER barrier = {
		.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		.Transition = {
			.pResource = renderTargets[frameIndex].Get(),
			.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			.StateBefore = D3D12_RESOURCE_STATE_PRESENT,
			.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET
		}
	};

	commandList->ResourceBarrier(1, &barrier);

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
	const D3D12_RESOURCE_BARRIER barrier = {
		.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		.Transition = {
			.pResource = renderTargets[frameIndex].Get(),
			.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
			.StateAfter = D3D12_RESOURCE_STATE_PRESENT,
		}
	};

	commandList->ResourceBarrier(1, &barrier);

	if (FAILED(commandList->Close()))
		throw std::runtime_error("Failed to close command list");

	ID3D12CommandList* lists[] = {commandList.Get()};

	commandQueue->ExecuteCommandLists(1, lists);

	BOOL fullscreen = FALSE;
	if (tearingSupported && FAILED(swapChain->GetFullscreenState(&fullscreen, nullptr)))
		throw std::runtime_error("Failed to query swap chain fullscreen state");
	const UINT presentFlags = tearingSupported && !fullscreen ? DXGI_PRESENT_ALLOW_TEARING : 0u;
	if (FAILED(swapChain->Present(0, presentFlags)))
		throw std::runtime_error("Failed to present frame");

	const UINT64 value = fenceValue++;

	if (FAILED(commandQueue->Signal(fence.Get(), value)))
		throw std::runtime_error("Failed to signal fence");

	fenceValues[frameIndex] = value;
}

void Renderer::CreateVertexBuffer()
{
	constexpr UINT bufferSize = sizeof(cubeVertices);

	constexpr D3D12_HEAP_PROPERTIES heapProperties = {
		.Type = D3D12_HEAP_TYPE_UPLOAD,
	};

	constexpr D3D12_RESOURCE_DESC bufferDesc = {
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Width = bufferSize,
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.SampleDesc = {.Count = 1},
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
	};

	if (FAILED(device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertexBuffer)
	)))
	{
		throw std::runtime_error("Failed to create vertex buffer");
	}

	void* mappedData = nullptr;
	constexpr D3D12_RANGE readRange = {.Begin = 0, .End = 0};

	if (FAILED(vertexBuffer->Map(0, &readRange, &mappedData)))
	{
		throw std::runtime_error("Failed to map vertex buffer");
	}

	memcpy(mappedData, cubeVertices, bufferSize);

	vertexBuffer->Unmap(0, nullptr);

	vertexBufferView = {
		.BufferLocation = vertexBuffer->GetGPUVirtualAddress(),
		.SizeInBytes = bufferSize,
		.StrideInBytes = sizeof(Vertex),
	};
}

void Renderer::CreateTextureDescriptorHeap()
{
	const D3D12_DESCRIPTOR_HEAP_DESC heap = {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		.NumDescriptors = TextureCapacity,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
	};

	if (FAILED(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&textureHeap))))
		throw std::runtime_error("Failed to create texture descriptor heap");

	textureDescriptorSize = device->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	);
}

void Renderer::RegisterTexture(const Texture& texture)
{
	if (textures.contains(texture.id))
		throw std::runtime_error("Texture ID is already registered");

	if (textures.size() == TextureCapacity)
		throw std::runtime_error("Texture descriptor heap is full");

	if (texture.pixels.size() != texture.width * texture.height * 4)
		throw std::runtime_error("Texture pixels must contain RGBA data");

	const D3D12_HEAP_PROPERTIES defaultHeap = {
		.Type = D3D12_HEAP_TYPE_DEFAULT,
	};
	const D3D12_RESOURCE_DESC textureDesc = {
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Width = texture.width,
		.Height = texture.height,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.SampleDesc = {.Count = 1},
	};

	GpuTexture gpuTexture;
	if (FAILED(device->CreateCommittedResource(
		&defaultHeap,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&gpuTexture.resource)
	)))
		throw std::runtime_error("Failed to create GPU texture");

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
	UINT rowCount = 0;
	UINT64 rowSize = 0;
	UINT64 uploadSize = 0;
	device->GetCopyableFootprints(
		&textureDesc,
		0,
		1,
		0,
		&footprint,
		&rowCount,
		&rowSize,
		&uploadSize
	);

	const D3D12_HEAP_PROPERTIES uploadHeap = {
		.Type = D3D12_HEAP_TYPE_UPLOAD,
	};
	const D3D12_RESOURCE_DESC uploadDesc = {
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Width = uploadSize,
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.SampleDesc = {.Count = 1},
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
	};

	ComPtr<ID3D12Resource> uploadBuffer;
	if (FAILED(device->CreateCommittedResource(
		&uploadHeap,
		D3D12_HEAP_FLAG_NONE,
		&uploadDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&uploadBuffer)
	)))
		throw std::runtime_error("Failed to create texture upload buffer");

	void* destination = nullptr;
	constexpr D3D12_RANGE readRange = {.Begin = 0, .End = 0};
	if (FAILED(uploadBuffer->Map(0, &readRange, &destination)))
		throw std::runtime_error("Failed to map texture upload buffer");

	const UINT sourceRowPitch = texture.width * 4;
	for (UINT row = 0; row < rowCount; ++row)
	{
		memcpy(
			static_cast<std::uint8_t*>(destination) + footprint.Offset + row * footprint.Footprint.RowPitch,
			texture.pixels.data() + row * sourceRowPitch,
			sourceRowPitch
		);
	}
	uploadBuffer->Unmap(0, nullptr);

	if (FAILED(commandAllocators[0]->Reset()))
		throw std::runtime_error("Failed to reset texture upload allocator");

	if (FAILED(commandList->Reset(commandAllocators[0].Get(), nullptr)))
		throw std::runtime_error("Failed to reset texture upload command list");

	const D3D12_TEXTURE_COPY_LOCATION destinationLocation = {
		.pResource = gpuTexture.resource.Get(),
		.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
		.SubresourceIndex = 0,
	};
	const D3D12_TEXTURE_COPY_LOCATION sourceLocation = {
		.pResource = uploadBuffer.Get(),
		.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
		.PlacedFootprint = footprint,
	};
	commandList->CopyTextureRegion(&destinationLocation, 0, 0, 0, &sourceLocation, nullptr);

	const D3D12_RESOURCE_BARRIER barrier = {
		.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		.Transition = {
			.pResource = gpuTexture.resource.Get(),
			.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
			.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		},
	};
	commandList->ResourceBarrier(1, &barrier);

	if (FAILED(commandList->Close()))
		throw std::runtime_error("Failed to close texture upload command list");

	ID3D12CommandList* lists[] = {commandList.Get()};
	commandQueue->ExecuteCommandLists(1, lists);
	WaitForGPU();

	gpuTexture.descriptorIndex = static_cast<UINT>(textures.size());
	D3D12_CPU_DESCRIPTOR_HANDLE handle = textureHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += gpuTexture.descriptorIndex * textureDescriptorSize;
	device->CreateShaderResourceView(gpuTexture.resource.Get(), nullptr, handle);
	textures.emplace(texture.id, std::move(gpuTexture));
}

void Renderer::BindTextures(
	const BlockId blockId,
	const TextureId sideTexture,
	const TextureId topTexture,
	const TextureId bottomTexture,
	const float topTintRed,
	const float topTintGreen,
	const float topTintBlue
)
{
	if (!textures.contains(sideTexture) || !textures.contains(topTexture) || !textures.contains(bottomTexture))
		throw std::runtime_error("Textures must be registered before binding them to a block");

	blockTextures[blockId] = {
		.side = sideTexture,
		.top = topTexture,
		.bottom = bottomTexture,
		.topTintRed = topTintRed,
		.topTintGreen = topTintGreen,
		.topTintBlue = topTintBlue,
		.faces = {sideTexture, sideTexture, sideTexture, sideTexture, topTexture, bottomTexture},
	};
}

void Renderer::BindFaceTextures(BlockId blockId, const std::array<TextureId, 6>& faces)
{
	for (const auto texture : faces)
		if (!textures.contains(texture)) throw std::runtime_error("Unregistered face texture");
	blockTextures.at(blockId).faces = faces;
}

void Renderer::BindSideOverlay(BlockId blockId, TextureId texture)
{
	if (!textures.contains(texture)) throw std::runtime_error("Unregistered side overlay texture");
	blockTextures.at(blockId).sideOverlay = texture;
}

void Renderer::SetBlockCracking(BlockPosition position, std::optional<TextureId> texture)
{
	crackingPosition = position;
	crackingTexture = texture;
}

void Renderer::CreateDepthBuffer()
{
	const auto backBuffer = renderTargets[0]->GetDesc();
	width = static_cast<UINT>(backBuffer.Width);
	height = backBuffer.Height;

	const D3D12_DESCRIPTOR_HEAP_DESC heap = {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		.NumDescriptors = 1,
	};
	if (FAILED(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&depthHeap))))
		throw std::runtime_error("Failed to create depth descriptor heap");

	const D3D12_HEAP_PROPERTIES properties = {
		.Type = D3D12_HEAP_TYPE_DEFAULT,
	};
	const D3D12_RESOURCE_DESC desc = {
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Width = width,
		.Height = height,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_D32_FLOAT,
		.SampleDesc = {.Count = 1},
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
	};

	constexpr D3D12_CLEAR_VALUE clear = {
		.Format = DXGI_FORMAT_D32_FLOAT,
		.DepthStencil = {.Depth = 1.0f, .Stencil = 0}
	};

	if (FAILED(
			device->CreateCommittedResource(
				&properties,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				&clear,
				IID_PPV_ARGS(&depthBuffer)
			))
	)
		throw std::runtime_error("Failed to create depth buffer");

	device->CreateDepthStencilView(
		depthBuffer.Get(),
		nullptr,
		depthHeap->GetCPUDescriptorHandleForHeapStart()
	);
}

void Renderer::CreatePipeline()
{
	constexpr D3D12_DESCRIPTOR_RANGE textureRanges[] = {
		{
			.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
			.NumDescriptors = 1,
			.BaseShaderRegister = 0,
			.RegisterSpace = 0,
			.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
		},
		{
			.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
			.NumDescriptors = 1,
			.BaseShaderRegister = 1,
			.RegisterSpace = 0,
			.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
		},
		{
			.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
			.NumDescriptors = 1,
			.BaseShaderRegister = 2,
			.RegisterSpace = 0,
			.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
		},
		{
			.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
			.NumDescriptors = 1,
			.BaseShaderRegister = 3,
			.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
		},
		{
			.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
			.NumDescriptors = 1,
			.BaseShaderRegister = 4,
			.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
		},
	};
	const D3D12_ROOT_PARAMETER parameters[] = {
		{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
			.Constants = {
				.ShaderRegister = 0,
				.RegisterSpace = 0,
				.Num32BitValues = 32,
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
		},
		{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable = {
				.NumDescriptorRanges = 1,
				.pDescriptorRanges = &textureRanges[0],
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
		},
		{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable = {
				.NumDescriptorRanges = 1,
				.pDescriptorRanges = &textureRanges[1],
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
		},
		{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable = {
				.NumDescriptorRanges = 1,
				.pDescriptorRanges = &textureRanges[2],
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
		},
		{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable = {.NumDescriptorRanges = 1, .pDescriptorRanges = &textureRanges[3]},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
		},
		{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable = {.NumDescriptorRanges = 1, .pDescriptorRanges = &textureRanges[4]},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
		},
	};
	constexpr D3D12_STATIC_SAMPLER_DESC sampler = {
		.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT,
		.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		.MaxLOD = D3D12_FLOAT32_MAX,
		.ShaderRegister = 0,
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
	};
	const D3D12_ROOT_SIGNATURE_DESC rootDesc = {
		.NumParameters = std::size(parameters),
		.pParameters = parameters,
		.NumStaticSamplers = 1,
		.pStaticSamplers = &sampler,
		.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
	};

	ComPtr<ID3DBlob> signature, errors;
	if (FAILED(D3D12SerializeRootSignature(
			&rootDesc,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&signature,
			&errors)
	))
		throw std::runtime_error("Failed to serialize root signature");

	if (FAILED(
			device->CreateRootSignature(
				0,
				signature->GetBufferPointer(),
				signature->GetBufferSize(),
				IID_PPV_ARGS(&rootSignature)
			))
	)
		throw std::runtime_error("Failed to create root signature");


	ComPtr<ID3DBlob> vs, ps;
	auto compile = [&](const char* entry, const char* target, ID3DBlob** output)
	{
		errors.Reset();
		const HRESULT result = D3DCompile(
			cubeShader,
			sizeof(cubeShader) - 1,
			nullptr,
			nullptr,
			nullptr,
			entry,
			target,
			0,
			0,
			output,
			&errors
		);

		if (FAILED(result))
		{
			if (errors)
				throw std::runtime_error(static_cast<const char*>(errors->GetBufferPointer()));

			throw std::runtime_error("Failed to compile shader");
		}
	};
	compile("VS", "vs_5_0", vs.GetAddressOf());
	compile("PS", "ps_5_0", ps.GetAddressOf());

	const D3D12_INPUT_ELEMENT_DESC input[] = {
        {
            .SemanticName = "TEXCOORD", .SemanticIndex = 2,
            .Format = DXGI_FORMAT_R32G32B32_FLOAT, .InputSlot = 1,
            .AlignedByteOffset = 0,
            .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
            .InstanceDataStepRate = 1
        },
		{
			.SemanticName = "TEXCOORD",
			.SemanticIndex = 1,
			.Format = DXGI_FORMAT_R32_UINT,
			.InputSlot = 0,
			.AlignedByteOffset = 20,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0,
		},
		{
			.SemanticName = "POSITION",
			.SemanticIndex = 0,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 0,
			.AlignedByteOffset = 0,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0,
		},
		{
			.SemanticName = "TEXCOORD",
			.SemanticIndex = 0,
			.Format = DXGI_FORMAT_R32G32_FLOAT,
			.InputSlot = 0,
			.AlignedByteOffset = 12,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0,
		},
	};

	const D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {
		.pRootSignature = rootSignature.Get(),
		.VS = {
			.pShaderBytecode = vs->GetBufferPointer(),
			.BytecodeLength = vs->GetBufferSize()
		},
		.PS = {
			.pShaderBytecode = ps->GetBufferPointer(),
			.BytecodeLength = ps->GetBufferSize()
		},
		.BlendState = {
			.RenderTarget = {
				{
					.SrcBlend = D3D12_BLEND_ONE, .DestBlend = D3D12_BLEND_ZERO,
					.BlendOp = D3D12_BLEND_OP_ADD, .SrcBlendAlpha = D3D12_BLEND_ONE,
					.DestBlendAlpha = D3D12_BLEND_ZERO, .BlendOpAlpha = D3D12_BLEND_OP_ADD,
					.LogicOp = D3D12_LOGIC_OP_NOOP, .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
				}
			}
		},
		.SampleMask = UINT_MAX,
		.RasterizerState = {
			.FillMode = D3D12_FILL_MODE_SOLID,
			.CullMode = D3D12_CULL_MODE_NONE,
			.DepthClipEnable = TRUE
		},
		.DepthStencilState = {
			.DepthEnable = TRUE,
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
			.DepthFunc = D3D12_COMPARISON_FUNC_LESS
		},
		.InputLayout = {
			.pInputElementDescs = input,
			.NumElements = std::size(input)
		},
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.NumRenderTargets = 1,
		.RTVFormats = {
			DXGI_FORMAT_B8G8R8A8_UNORM
		},
		.DSVFormat = DXGI_FORMAT_D32_FLOAT,
		.SampleDesc = {
			.Count = 1
		},
	};

	if (FAILED(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pipelineState))))
		throw std::runtime_error("Failed to create graphics pipeline");
}

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
        visibleChunks.push({x * x + z * z, chunk});
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
                const D3D12_HEAP_PROPERTIES heap{.Type = D3D12_HEAP_TYPE_UPLOAD};
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
            const D3D12_RANGE noRead{.Begin = 0, .End = 0};
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
