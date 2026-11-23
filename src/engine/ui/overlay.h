#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <string_view>
#include <array>
#include <optional>
#include "api/game.h"
#include <DirectXMath.h>

using namespace Microsoft::WRL;

class Texture;

class Overlay
{
	public:
		explicit Overlay(ID3D12Device* device);
		void Draw(ID3D12GraphicsCommandList* commands, UINT frameIndex,
		          UINT width, UINT height, std::string_view text, bool centered = false) const;
		void DrawLines(ID3D12GraphicsCommandList* commands, UINT frameIndex, UINT width, UINT height,
		               const DirectX::XMFLOAT4X4& matrix, const std::vector<DebugLine>& lines) const;
		void DrawImage(ID3D12GraphicsCommandList* commands, UINT frameIndex,
		               UINT width, UINT height, const Texture& image) const;

	private:

        struct Vertex { float x, y, r, g, b, a; };
        struct Rgba { float r, g, b, a; };
        struct PixelRect { float x, y, width, height; };
        struct TextLayout { float left, panelWidth, panelHeight; };
        static constexpr std::size_t MaxCharacters = 256;
        static constexpr UINT BufferBytes = (MaxCharacters * 35 * 6 + 6) * sizeof(Vertex);
        static constexpr float Scale = 2, Advance = 6 * Scale, LineHeight = 10 * Scale;
        static constexpr float Padding = 12, Margin = 16;

        void CreateRootSignature(ID3D12Device* const device);
        void CreatePipeline(ID3D12Device* const device);
        void CreateVertexBuffers(ID3D12Device* const device);
        static TextLayout MeasureText(const std::string_view text, const UINT width, const bool centered);
        static std::vector<Vertex> BuildTextVertices(const UINT width, const UINT height, const std::string_view text, const bool centered);
        static void AppendText(std::vector<Vertex>& vertices, const UINT width, const UINT height, const std::string_view text, const float left);
        static void AppendGlyph(std::vector<Vertex>& vertices, const UINT width, const UINT height, const char character, const Vec2 position);
        static void AppendRectangle(std::vector<Vertex>& vertices, const UINT width, const UINT height, const PixelRect rectangle, const Rgba color);
        static void AppendQuad(std::vector<Vertex>& vertices, const std::array<Vec2,4>& points, const Rgba color);
        static std::vector<Vertex> BuildImageVertices(const UINT width, const UINT height, const Texture& image);
        static std::vector<Vertex> BuildLineVertices(const UINT width, const UINT height, const DirectX::XMFLOAT4X4& matrix, const std::vector<DebugLine>& lines);
        static bool ClipLineToPlane(DirectX::XMFLOAT4& a, DirectX::XMFLOAT4& b, const float da, const float db);
        static std::optional<std::array<Vec2,4>> ProjectLine(const DebugLine& line, const DirectX::XMFLOAT4X4& matrix, const UINT width, const UINT height);
        void DrawVertices(ID3D12GraphicsCommandList* const commands, const UINT frameIndex, const std::vector<Vertex>& vertices, const char* const error) const;


		ComPtr<ID3D12RootSignature> rootSignature;
		ComPtr<ID3D12PipelineState> pipeline;
		ComPtr<ID3D12Resource> buffers[2];
};
