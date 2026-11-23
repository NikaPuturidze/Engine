#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <string_view>

using namespace Microsoft::WRL;

class Texture;

class Overlay
{
	public:
		explicit Overlay(ID3D12Device* device);
		void Draw(ID3D12GraphicsCommandList* commands, UINT frameIndex,
		          UINT width, UINT height, std::string_view text, bool centered = false) const;
		void DrawImage(ID3D12GraphicsCommandList* commands, UINT frameIndex,
		               UINT width, UINT height, const Texture& image) const;

	private:
		ComPtr<ID3D12RootSignature> rootSignature;
		ComPtr<ID3D12PipelineState> pipeline;
		ComPtr<ID3D12Resource> buffers[2];
};
