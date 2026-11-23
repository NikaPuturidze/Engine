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

	constexpr D3D12_DESCRIPTOR_HEAP_DESC heap = {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		.NumDescriptors = 1,
	};
	if (FAILED(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&depthHeap))))
		throw std::runtime_error("Failed to create depth descriptor heap");

	constexpr D3D12_HEAP_PROPERTIES properties = {
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
	CreateTerrainRootSignature();
	const auto vertexShader = CompileTerrainShader("VS", "vs_5_0");
	const auto pixelShader = CompileTerrainShader("PS", "ps_5_0");
	CreateTerrainPipelineState(vertexShader.Get(), pixelShader.Get());
}

void Renderer::CreateTerrainRootSignature()
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
}

ComPtr<ID3DBlob> Renderer::CompileTerrainShader(const char* const entry, const char* const target)
{
	ComPtr<ID3DBlob> shader, errors;
	const auto result = D3DCompile(cubeShader, sizeof(cubeShader) - 1, nullptr, nullptr, nullptr,
	                               entry, target, 0, 0, &shader, &errors);
	if (FAILED(result))
	{
		if (errors) throw std::runtime_error(static_cast<const char*>(errors->GetBufferPointer()));
		throw std::runtime_error("Failed to compile shader");
	}
	return shader;
}

void Renderer::CreateTerrainPipelineState(ID3DBlob* const vs, ID3DBlob* const ps)
{
	constexpr D3D12_INPUT_ELEMENT_DESC input[] = {
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
