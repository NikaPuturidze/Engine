#include "engine/ui/overlay.h"
#include "engine/graphics/texture.h"

#include <d3dcompiler.h>
#include <array>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace
{
	void Check(const HRESULT result, const char* message)
	{
		if (FAILED(result)) throw std::runtime_error(message);
	}

	std::array<unsigned char, 7> Glyph(char character)
	{
		if (character >= 'a' && character <= 'z') character -= 'a' - 'A';
		switch (character)
		{
			case 'A': return {14, 17, 17, 31, 17, 17, 17};
			case 'B': return {30, 17, 17, 30, 17, 17, 30};
			case 'C': return {14, 17, 16, 16, 16, 17, 14};
			case 'D': return {30, 17, 17, 17, 17, 17, 30};
			case 'E': return {31, 16, 16, 30, 16, 16, 31};
			case 'F': return {31, 16, 16, 30, 16, 16, 16};
			case 'G': return {14, 17, 16, 23, 17, 17, 14};
			case 'H': return {17, 17, 17, 31, 17, 17, 17};
			case 'I': return {14, 4, 4, 4, 4, 4, 14};
			case 'J': return {7, 2, 2, 2, 2, 18, 12};
			case 'K': return {17, 18, 20, 24, 20, 18, 17};
			case 'L': return {16, 16, 16, 16, 16, 16, 31};
			case 'M': return {17, 27, 21, 21, 17, 17, 17};
			case 'N': return {17, 25, 21, 19, 17, 17, 17};
			case 'O': return {14, 17, 17, 17, 17, 17, 14};
			case 'P': return {30, 17, 17, 30, 16, 16, 16};
			case 'Q': return {14, 17, 17, 17, 21, 18, 13};
			case 'R': return {30, 17, 17, 30, 20, 18, 17};
			case 'S': return {15, 16, 16, 14, 1, 1, 30};
			case 'T': return {31, 4, 4, 4, 4, 4, 4};
			case 'U': return {17, 17, 17, 17, 17, 17, 14};
			case 'V': return {17, 17, 17, 17, 17, 10, 4};
			case 'W': return {17, 17, 17, 21, 21, 21, 10};
			case 'X': return {17, 17, 10, 4, 10, 17, 17};
			case 'Y': return {17, 17, 10, 4, 4, 4, 4};
			case 'Z': return {31, 1, 2, 4, 8, 16, 31};
			case '0': return {14, 17, 19, 21, 25, 17, 14};
			case '1': return {4, 12, 4, 4, 4, 4, 14};
			case '2': return {14, 17, 1, 2, 4, 8, 31};
			case '3': return {30, 1, 1, 14, 1, 1, 30};
			case '4': return {2, 6, 10, 18, 31, 2, 2};
			case '5': return {31, 16, 16, 30, 1, 1, 30};
			case '6': return {14, 16, 16, 30, 17, 17, 14};
			case '7': return {31, 1, 2, 4, 8, 8, 8};
			case '8': return {14, 17, 17, 14, 17, 17, 14};
			case '9': return {14, 17, 17, 15, 1, 1, 14};
			case '-': return {0, 0, 0, 31, 0, 0, 0};
			case '+': return {0, 4, 4, 31, 4, 4, 0};
			case '.': return {0, 0, 0, 0, 0, 12, 12};
			case ',': return {0, 0, 0, 0, 0, 4, 8};
			case ':': return {0, 12, 12, 0, 12, 12, 0};
			case '/': return {1, 2, 2, 4, 8, 8, 16};
			case ' ': return {};
			default: return {14, 17, 1, 2, 4, 0, 4};
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


Overlay::Overlay(ID3D12Device* const device)
{
    CreateRootSignature(device);
    CreatePipeline(device);
    CreateVertexBuffers(device);
}

void Overlay::CreateRootSignature(ID3D12Device* const device)
{
	constexpr D3D12_ROOT_SIGNATURE_DESC rootDesc{
		.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	};
	Microsoft::WRL::ComPtr<ID3DBlob> signature, errors;
	Check(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1,
	                                  &signature, &errors), "Failed to serialize overlay root signature");
	Check(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
	                                  IID_PPV_ARGS(&rootSignature)), "Failed to create overlay root signature");


}

void Overlay::CreatePipeline(ID3D12Device* const device)
{
	Microsoft::WRL::ComPtr<ID3DBlob> vs, ps, errors;
	auto compile = [&](const char* entry, const char* target, ID3DBlob** output)
	{
		errors.Reset();
		if (FAILED(D3DCompile(Shader, sizeof(Shader) - 1, nullptr, nullptr, nullptr,
			entry, target, 0, 0, output, &errors)))
			throw std::runtime_error(errors
				                         ? static_cast<const char*>(errors->GetBufferPointer())
				                         : "Failed to compile overlay shader");
	};

	compile("VS", "vs_5_0", vs.GetAddressOf());
	compile("PS", "ps_5_0", ps.GetAddressOf());

	constexpr D3D12_INPUT_ELEMENT_DESC input[] = {
		{
			.SemanticName = "POSITION",
			.SemanticIndex = 0,
			.Format = DXGI_FORMAT_R32G32_FLOAT,
			.InputSlot = 0,
			.AlignedByteOffset = 0,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0
		},
		{
			.SemanticName = "COLOR",
			.SemanticIndex = 0,
			.Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
			.InputSlot = 0,
			.AlignedByteOffset = 8,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0
		}
	};
	const D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{
		.pRootSignature = rootSignature.Get(),
		.VS = {.pShaderBytecode = vs->GetBufferPointer(), .BytecodeLength = vs->GetBufferSize()},
		.PS = {.pShaderBytecode = ps->GetBufferPointer(), .BytecodeLength = ps->GetBufferSize()},
		.BlendState = {
			.RenderTarget = {
				{
					.BlendEnable = TRUE,
					.SrcBlend = D3D12_BLEND_SRC_ALPHA,
					.DestBlend = D3D12_BLEND_INV_SRC_ALPHA,
					.BlendOp = D3D12_BLEND_OP_ADD,
					.SrcBlendAlpha = D3D12_BLEND_ONE,
					.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA,
					.BlendOpAlpha = D3D12_BLEND_OP_ADD,
					.LogicOp = D3D12_LOGIC_OP_NOOP,
					.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
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
			.DepthEnable = FALSE,
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
			.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS
		},
		.InputLayout = {.pInputElementDescs = input, .NumElements = 2},
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.NumRenderTargets = 1,
		.RTVFormats = {DXGI_FORMAT_B8G8R8A8_UNORM},
		.DSVFormat = DXGI_FORMAT_D32_FLOAT,
		.SampleDesc = {.Count = 1}
	};
	Check(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline)),
	      "Failed to create overlay pipeline");


}

void Overlay::CreateVertexBuffers(ID3D12Device* const device)
{
	constexpr D3D12_HEAP_PROPERTIES heap{
		.Type = D3D12_HEAP_TYPE_UPLOAD
	};
	constexpr D3D12_RESOURCE_DESC buffer{
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER, .Width = BufferBytes,
		.Height = 1, .DepthOrArraySize = 1, .MipLevels = 1,
		.SampleDesc = {.Count = 1}, .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR
	};

	for (auto& resource : buffers)
		Check(device->CreateCommittedResource(
			      &heap,
			      D3D12_HEAP_FLAG_NONE,
			      &buffer,
			      D3D12_RESOURCE_STATE_GENERIC_READ,
			      nullptr,
			      IID_PPV_ARGS(&resource)),
		      "Failed to create overlay vertex buffer"
		);

}

void Overlay::Draw(ID3D12GraphicsCommandList* const commands, const UINT frameIndex,
    const UINT width, const UINT height, const std::string_view text, const bool centered) const
{
    if (text.empty() || width == 0 || height == 0) return;
    const auto vertices = BuildTextVertices(width, height, text.substr(0, MaxCharacters), centered);
    DrawVertices(commands, frameIndex, vertices, "Failed to map overlay vertices");
}

Overlay::TextLayout Overlay::MeasureText(const std::string_view text, const UINT width, const bool centered)
{
    std::size_t columns = 0, lineColumns = 0, lines = 1;
    for (const char character : text)
    {
        if (character == '\n')
        {
            columns = std::max(columns, lineColumns);
            lineColumns = 0;
            ++lines;
        }
        else ++lineColumns;
    }
    columns = std::max(columns, lineColumns);
    const float panelWidth = static_cast<float>(columns) * Advance + Padding * 2;
    return {
        .left = std::max(0.0f, centered ? (static_cast<float>(width) - panelWidth) * 0.5f :
            static_cast<float>(width) - Margin - panelWidth),
        .panelWidth = panelWidth,
        .panelHeight = static_cast<float>(lines) * LineHeight + Padding * 2 - 6
    };
}

std::vector<Overlay::Vertex> Overlay::BuildTextVertices(const UINT width, const UINT height,
    const std::string_view text, const bool centered)
{
    const auto layout = MeasureText(text, width, centered);
    std::vector<Vertex> vertices;
    vertices.reserve(text.size() * 35 * 6 + 6);
    AppendRectangle(vertices, width, height, {layout.left, Margin, layout.panelWidth, layout.panelHeight},
        {0.035f, 0.05f, 0.075f, 0.82f});
    AppendText(vertices, width, height, text, layout.left + Padding);
    return vertices;
}

void Overlay::AppendText(std::vector<Vertex>& vertices, const UINT width, const UINT height,
    const std::string_view text, const float left)
{
    Vec2 position{left, Margin + Padding};
    for (const char character : text)
    {
        if (character == '\n')
        {
            position.x = left;
            position.y += LineHeight;
            continue;
        }
        AppendGlyph(vertices, width, height, character, position);
        position.x += Advance;
    }
}

void Overlay::AppendGlyph(std::vector<Vertex>& vertices, const UINT width, const UINT height,
    const char character, const Vec2 position)
{
    const auto glyph = Glyph(character);
    for (int row = 0; row < 7; ++row)
        for (int column = 0; column < 5; ++column)
            if (glyph[row] & (1 << (4 - column)))
                AppendRectangle(vertices, width, height,
                    {position.x + column * Scale, position.y + row * Scale, Scale, Scale}, {0.9f, 0.95f, 1, 1});
}

void Overlay::AppendRectangle(std::vector<Vertex>& vertices, const UINT width, const UINT height,
    const PixelRect rectangle, const Rgba color)
{
    const float x0 = rectangle.x * 2 / width - 1;
    const float x1 = (rectangle.x + rectangle.width) * 2 / width - 1;
    const float y0 = 1 - rectangle.y * 2 / height;
    const float y1 = 1 - (rectangle.y + rectangle.height) * 2 / height;
    AppendQuad(vertices, {{{x0,y0}, {x1,y0}, {x0,y1}, {x1,y1}}}, color);
}

void Overlay::AppendQuad(std::vector<Vertex>& vertices, const std::array<Vec2, 4>& points, const Rgba color)
{
    constexpr unsigned int order[] = {0, 1, 2, 2, 1, 3};
    for (const auto index : order)
    {
        vertices.push_back(
            {
                .x = points[index].x,
                .y = points[index].y,
                .r = color.r,
                .g = color.g,
                .b = color.b,
                .a = color.a
            }
        );
    }
}

void Overlay::DrawImage(ID3D12GraphicsCommandList* const commands, const UINT frameIndex,
    const UINT width, const UINT height, const Texture& image) const
{
    if (width == 0 || height == 0) return;
    if (static_cast<std::uint64_t>(image.width) * image.height * 6 * sizeof(Vertex) > BufferBytes)
        throw std::runtime_error("Overlay image exceeds vertex buffer capacity");
    const auto vertices = BuildImageVertices(width, height, image);
    DrawVertices(commands, frameIndex, vertices, "Failed to map overlay image vertices");
}

std::vector<Overlay::Vertex> Overlay::BuildImageVertices(const UINT width, const UINT height, const Texture& image)
{
    const float left = (static_cast<float>(width) - image.width * Scale) * 0.5f;
    const float top = (static_cast<float>(height) - image.height * Scale) * 0.5f;
    std::vector<Vertex> vertices;
    vertices.reserve(image.width * image.height * 6);
    for (UINT y = 0; y < image.height; ++y)
        for (UINT x = 0; x < image.width; ++x)
        {
            const auto* pixel = &image.pixels[(static_cast<std::size_t>(y) * image.width + x) * 4];
            if (pixel[3] == 0) continue;
            AppendRectangle(vertices, width, height, {left + x * Scale, top + y * Scale, Scale, Scale},
                {pixel[0] / 255.0f, pixel[1] / 255.0f, pixel[2] / 255.0f, pixel[3] / 255.0f});
        }
    return vertices;
}

void Overlay::DrawLines(ID3D12GraphicsCommandList* const commands, const UINT frameIndex, const UINT width,
    const UINT height, const DirectX::XMFLOAT4X4& matrix, const std::vector<DebugLine>& lines) const
{
    if (lines.empty() || width == 0 || height == 0) return;
    const auto vertices = BuildLineVertices(width, height, matrix, lines);
    DrawVertices(commands, frameIndex, vertices, "Failed to map debug vertices");
}

std::vector<Overlay::Vertex> Overlay::BuildLineVertices(const UINT width, const UINT height,
    const DirectX::XMFLOAT4X4& matrix, const std::vector<DebugLine>& lines)
{
    std::vector<Vertex> vertices;
    for (const auto& line : lines)
    {
        if ((vertices.size() + 6) * sizeof(Vertex) > BufferBytes) break;
        const auto points = ProjectLine(line, matrix, width, height);
        if (!points) continue;
        AppendQuad(vertices, *points, {line.color.r, line.color.g, line.color.b, 1});
    }
    return vertices;
}

bool Overlay::ClipLineToPlane(DirectX::XMFLOAT4& a, DirectX::XMFLOAT4& b, const float da, const float db)
{
    if (da < 0 && db < 0) return false;
    if (da < 0 || db < 0)
    {
        const float t = da / (da - db);
        const DirectX::XMFLOAT4 point{a.x + (b.x-a.x)*t, a.y + (b.y-a.y)*t, a.z + (b.z-a.z)*t, a.w + (b.w-a.w)*t};
        if (da < 0) a = point;
        else b = point;
    }
    return true;
}

std::optional<std::array<Vec2, 4>> Overlay::ProjectLine(const DebugLine& line,
    const DirectX::XMFLOAT4X4& matrix, const UINT width, const UINT height)
{
    const auto transform = DirectX::XMLoadFloat4x4(&matrix);
    DirectX::XMFLOAT4 a, b;
    DirectX::XMStoreFloat4(&a, DirectX::XMVector4Transform(DirectX::XMVectorSet(line.from.x, line.from.y, line.from.z, 1), transform));
    DirectX::XMStoreFloat4(&b, DirectX::XMVector4Transform(DirectX::XMVectorSet(line.to.x, line.to.y, line.to.z, 1), transform));
    if (!ClipLineToPlane(a, b, a.w - 0.0001f, b.w - 0.0001f) || !ClipLineToPlane(a, b, a.z, b.z) ||
        !ClipLineToPlane(a, b, a.w - a.z, b.w - b.z)) return std::nullopt;
    const float ax = a.x/a.w, ay = a.y/a.w, bx = b.x/b.w, by = b.y/b.w;
    const float dx = (bx-ax)*width, dy = (by-ay)*height;
    const float length = std::hypot(dx, dy);
    if (length < 0.0001f) return std::nullopt;
    const float ox = -dy/length*2/width, oy = dx/length*2/height;
    return std::array<Vec2, 4>{{{ax+ox,ay+oy}, {bx+ox,by+oy}, {ax-ox,ay-oy}, {bx-ox,by-oy}}};
}

void Overlay::DrawVertices(ID3D12GraphicsCommandList* const commands, const UINT frameIndex,
    const std::vector<Vertex>& vertices, const char* const error) const
{
    if (vertices.empty()) return;
    const auto& buffer = buffers[frameIndex];
    void* mapped = nullptr;
    constexpr D3D12_RANGE noRead{0, 0};
    Check(buffer->Map(0, &noRead, &mapped), error);
    const UINT bytes = static_cast<UINT>(vertices.size() * sizeof(Vertex));
    std::memcpy(mapped, vertices.data(), bytes);
    const D3D12_RANGE written{0, bytes};
    buffer->Unmap(0, &written);
    const D3D12_VERTEX_BUFFER_VIEW view{
        .BufferLocation = buffer->GetGPUVirtualAddress(),
        .SizeInBytes = bytes,
        .StrideInBytes = sizeof(Vertex)
    };
    commands->SetPipelineState(pipeline.Get());
    commands->SetGraphicsRootSignature(rootSignature.Get());
    commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commands->IASetVertexBuffers(0, 1, &view);
    commands->DrawInstanced(static_cast<UINT>(vertices.size()), 1, 0, 0);
}
