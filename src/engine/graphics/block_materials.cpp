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

void Renderer::CreateTextureDescriptorHeap()
{
	constexpr D3D12_DESCRIPTOR_HEAP_DESC heap = {
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

	constexpr D3D12_HEAP_PROPERTIES defaultHeap = {
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

	constexpr D3D12_HEAP_PROPERTIES uploadHeap = {
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

void Renderer::BindFaceTextures(const BlockId blockId, const std::array<TextureId, 6>& faces)
{
	for (const auto texture : faces)
		if (!textures.contains(texture)) throw std::runtime_error("Unregistered face texture");
	blockTextures.at(blockId).faces = faces;
}

void Renderer::BindSideOverlay(const BlockId blockId, const TextureId texture)
{
	if (!textures.contains(texture)) throw std::runtime_error("Unregistered side overlay texture");
	blockTextures.at(blockId).sideOverlay = texture;
}

void Renderer::SetBlockCracking(const BlockPosition position, const std::optional<TextureId> texture)
{
	crackingPosition = position;
	crackingTexture = texture;
}
