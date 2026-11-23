#include "overlay.h"

#include <d3dcompiler.h>
#include <array>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace
{
    struct Vertex { float x, y, r, g, b, a; };
    constexpr std::size_t MaxCharacters = 256;
    constexpr UINT BufferBytes = (MaxCharacters * 35 * 6 + 6) * sizeof(Vertex);

    void Check(HRESULT result, const char* message)
    {
        if (FAILED(result)) throw std::runtime_error(message);
    }

    // Five columns, seven rows. Lowercase text uses the same readable pixel font.
    std::array<unsigned char, 7> Glyph(char character)
    {
        if (character >= 'a' && character <= 'z') character -= 'a' - 'A';
        switch (character)
        {
        case 'A': return {14,17,17,31,17,17,17};
        case 'B': return {30,17,17,30,17,17,30};
        case 'C': return {14,17,16,16,16,17,14};
        case 'D': return {30,17,17,17,17,17,30};
        case 'E': return {31,16,16,30,16,16,31};
        case 'F': return {31,16,16,30,16,16,16};
        case 'G': return {14,17,16,23,17,17,14};
        case 'H': return {17,17,17,31,17,17,17};
        case 'I': return {14,4,4,4,4,4,14};
        case 'J': return {7,2,2,2,2,18,12};
        case 'K': return {17,18,20,24,20,18,17};
        case 'L': return {16,16,16,16,16,16,31};
        case 'M': return {17,27,21,21,17,17,17};
        case 'N': return {17,25,21,19,17,17,17};
        case 'O': return {14,17,17,17,17,17,14};
        case 'P': return {30,17,17,30,16,16,16};
        case 'Q': return {14,17,17,17,21,18,13};
        case 'R': return {30,17,17,30,20,18,17};
        case 'S': return {15,16,16,14,1,1,30};
        case 'T': return {31,4,4,4,4,4,4};
        case 'U': return {17,17,17,17,17,17,14};
        case 'V': return {17,17,17,17,17,10,4};
        case 'W': return {17,17,17,21,21,21,10};
        case 'X': return {17,17,10,4,10,17,17};
        case 'Y': return {17,17,10,4,4,4,4};
        case 'Z': return {31,1,2,4,8,16,31};
        case '0': return {14,17,19,21,25,17,14};
        case '1': return {4,12,4,4,4,4,14};
        case '2': return {14,17,1,2,4,8,31};
        case '3': return {30,1,1,14,1,1,30};
        case '4': return {2,6,10,18,31,2,2};
        case '5': return {31,16,16,30,1,1,30};
        case '6': return {14,16,16,30,17,17,14};
        case '7': return {31,1,2,4,8,8,8};
        case '8': return {14,17,17,14,17,17,14};
        case '9': return {14,17,17,15,1,1,14};
        case '-': return {0,0,0,31,0,0,0};
        case '+': return {0,4,4,31,4,4,0};
        case '.': return {0,0,0,0,0,12,12};
        case ',': return {0,0,0,0,0,4,8};
        case ':': return {0,12,12,0,12,12,0};
        case '/': return {1,2,2,4,8,8,16};
        case ' ': return {};
        default: return {14,17,1,2,4,0,4};
        }
    }

    constexpr char Shader[] = R"(
struct Output { float4 position : SV_POSITION; float4 color : COLOR; };
Output VS(float2 position : POSITION, float4 color : COLOR) {
    Output output; output.position = float4(position, 0, 1);
    output.color = color; return output;
}
float4 PS(Output input) : SV_TARGET { return input.color; }
)";
}

Overlay::Overlay(ID3D12Device* device)
{
    const D3D12_ROOT_SIGNATURE_DESC rootDesc{
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    };
    Microsoft::WRL::ComPtr<ID3DBlob> signature, errors;
    Check(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        &signature, &errors), "Failed to serialize overlay root signature");
    Check(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)), "Failed to create overlay root signature");

    Microsoft::WRL::ComPtr<ID3DBlob> vs, ps;
    auto compile = [&](const char* entry, const char* target, ID3DBlob** output)
    {
        errors.Reset();
        if (FAILED(D3DCompile(Shader, sizeof(Shader) - 1, nullptr, nullptr, nullptr,
            entry, target, 0, 0, output, &errors)))
            throw std::runtime_error(errors ? static_cast<const char*>(errors->GetBufferPointer()) :
                "Failed to compile overlay shader");
    };
    compile("VS", "vs_5_0", vs.GetAddressOf());
    compile("PS", "ps_5_0", ps.GetAddressOf());
    const D3D12_INPUT_ELEMENT_DESC input[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature.Get();
    desc.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    desc.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    auto& blend = desc.BlendState.RenderTarget[0];
    blend.BlendEnable = TRUE;
    blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOp = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.LogicOp = D3D12_LOGIC_OP_NOOP;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.SampleMask = UINT_MAX;
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.DepthClipEnable = TRUE;
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    desc.InputLayout = {input, 2};
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    desc.SampleDesc.Count = 1;
    Check(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline)),
        "Failed to create overlay pipeline");

    const D3D12_HEAP_PROPERTIES heap{.Type = D3D12_HEAP_TYPE_UPLOAD};
    const D3D12_RESOURCE_DESC buffer{
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER, .Width = BufferBytes,
        .Height = 1, .DepthOrArraySize = 1, .MipLevels = 1,
        .SampleDesc = {.Count = 1}, .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR
    };
    for (auto& resource : buffers)
        Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource)),
            "Failed to create overlay vertex buffer");
}

void Overlay::Draw(ID3D12GraphicsCommandList* commands, UINT frameIndex,
                   UINT width, UINT height, std::string_view text)
{
    if (text.empty() || width == 0 || height == 0) return;
    text = text.substr(0, MaxCharacters);
    constexpr float Scale = 2, Advance = 6 * Scale, LineHeight = 10 * Scale;
    constexpr float Padding = 12, Margin = 16;
    std::size_t columns = 0, lineColumns = 0, lines = 1;
    for (char character : text)
    {
        if (character == '\n') { columns = (std::max)(columns, lineColumns); lineColumns = 0; ++lines; }
        else ++lineColumns;
    }
    columns = (std::max)(columns, lineColumns);
    const float panelWidth = static_cast<float>(columns) * Advance + Padding * 2;
    const float left = (std::max)(0.0f, static_cast<float>(width) - Margin - panelWidth);
    const float top = Margin;
    std::vector<Vertex> vertices;
    vertices.reserve(text.size() * 35 * 6 + 6);
    auto rectangle = [&](float x, float y, float w, float h, float r, float g, float b, float a)
    {
        const float x0 = x * 2 / width - 1, x1 = (x + w) * 2 / width - 1;
        const float y0 = 1 - y * 2 / height, y1 = 1 - (y + h) * 2 / height;
        const Vertex quad[] = {{x0,y0,r,g,b,a},{x1,y0,r,g,b,a},{x0,y1,r,g,b,a},
                               {x0,y1,r,g,b,a},{x1,y0,r,g,b,a},{x1,y1,r,g,b,a}};
        vertices.insert(vertices.end(), std::begin(quad), std::end(quad));
    };
    rectangle(left, top, panelWidth, static_cast<float>(lines) * LineHeight + Padding * 2 - 6,
              0.035f, 0.05f, 0.075f, 0.82f);
    float x = left + Padding, y = top + Padding;
    for (char character : text)
    {
        if (character == '\n') { x = left + Padding; y += LineHeight; continue; }
        const auto glyph = Glyph(character);
        for (int row = 0; row < 7; ++row)
            for (int column = 0; column < 5; ++column)
                if (glyph[row] & (1 << (4 - column)))
                    rectangle(x + column * Scale, y + row * Scale, Scale, Scale, 0.9f, 0.95f, 1, 1);
        x += Advance;
    }
    auto& buffer = buffers[frameIndex];
    void* mapped = nullptr;
    const D3D12_RANGE noRead{0, 0};
    Check(buffer->Map(0, &noRead, &mapped), "Failed to map overlay vertices");
    const UINT bytes = static_cast<UINT>(vertices.size() * sizeof(Vertex));
    std::memcpy(mapped, vertices.data(), bytes);
    const D3D12_RANGE written{0, bytes};
    buffer->Unmap(0, &written);
    const D3D12_VERTEX_BUFFER_VIEW view{buffer->GetGPUVirtualAddress(), bytes, sizeof(Vertex)};
    commands->SetPipelineState(pipeline.Get());
    commands->SetGraphicsRootSignature(rootSignature.Get());
    commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commands->IASetVertexBuffers(0, 1, &view);
    commands->DrawInstanced(static_cast<UINT>(vertices.size()), 1, 0, 0);
}
