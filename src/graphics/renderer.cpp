#include "renderer.h"
#include "meshes/cube_mesh.h"
#include "cube_shader.h"
#include <dxgi1_4.h>
#include <stdexcept>
#include <cstring>
#include <d3dcompiler.h>

Renderer::Renderer(const HWND hwnd)
{
	CreateAdapter();
	CreateDevice();
	CreateCommandQueue();
	CreateSwapChain(hwnd);
	CreateFenceSynchronization();
	CreateDescriptorHeap();
	CreateRenderTargets();
	CreateCommandResources();
	CreateVertexBuffer();
	CreateDepthBuffer();
	CreatePipeline();
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
	constexpr DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
		.Width = 0,
		.Height = 0,
		.Format = DXGI_FORMAT_B8G8R8A8_UNORM,
		.SampleDesc = {
			.Count = 1
		},
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = 2,
		.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
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
		throw std::runtime_error("Failed to create swap chain");

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

void Renderer::BeginFrame(const Camera& camera)
{
	DirectX::XMStoreFloat4x4(
		&viewProjection,
		camera.GetViewMatrix() * camera.GetProjectionMatrix(static_cast<float>(width) / height)
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

	if (FAILED(swapChain->Present(1, 0)))
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
		.SampleDesc = { .Count = 1 },
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
	};

	constexpr D3D12_CLEAR_VALUE clear = {
		.Format = DXGI_FORMAT_D32_FLOAT,
		.DepthStencil = { .Depth = 1.0f, .Stencil = 0 }
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
	constexpr D3D12_ROOT_PARAMETER parameter = {
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
		.Constants = {
			.ShaderRegister = 0,
			.RegisterSpace = 0,
			.Num32BitValues = 16,
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX,
	};
	const D3D12_ROOT_SIGNATURE_DESC rootDesc = {
		.NumParameters = 1,
		.pParameters = &parameter,
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

	const D3D12_INPUT_ELEMENT_DESC input = {
		.SemanticName = "POSITION",
		.SemanticIndex = 0,
		.Format = DXGI_FORMAT_R32G32B32_FLOAT,
		.InputSlot = 0,
		.AlignedByteOffset = 0,
		.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		.InstanceDataStepRate = 0,
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
        .BlendState = { .RenderTarget = {{
            .SrcBlend = D3D12_BLEND_ONE, .DestBlend = D3D12_BLEND_ZERO,
            .BlendOp = D3D12_BLEND_OP_ADD, .SrcBlendAlpha = D3D12_BLEND_ONE,
            .DestBlendAlpha = D3D12_BLEND_ZERO, .BlendOpAlpha = D3D12_BLEND_OP_ADD,
            .LogicOp = D3D12_LOGIC_OP_NOOP, .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
        }} },
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
        	.pInputElementDescs = &input,
        	.NumElements = 1
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

void Renderer::DrawCube(const Cube& cube) const
{
	using namespace DirectX;
	const XMMATRIX world = XMMatrixScaling(cube.size * 0.5f, cube.size * 0.5f, cube.size * 0.5f)
		* XMMatrixTranslation(cube.x, cube.y, cube.z);
	const XMMATRIX transform = world * XMLoadFloat4x4(&viewProjection);
	XMFLOAT4X4 matrix;
	XMStoreFloat4x4(&matrix, transform);
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetGraphicsRootSignature(rootSignature.Get());
	commandList->SetGraphicsRoot32BitConstants(0, 16, &matrix, 0);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
	commandList->DrawInstanced(
		static_cast<UINT>(std::size(cubeVertices)),
		1,
		0,
		0
	);
}
