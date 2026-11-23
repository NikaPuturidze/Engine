#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <string_view>

// One top-right text panel per frame. Call after all world drawing.
class Overlay
{
public:
    explicit Overlay(ID3D12Device* device);
    void Draw(ID3D12GraphicsCommandList* commands, UINT frameIndex,
              UINT width, UINT height, std::string_view text);

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline;
    // Match the renderer's two swap-chain frames; BeginFrame waits before reuse.
    Microsoft::WRL::ComPtr<ID3D12Resource> buffers[2];
};
