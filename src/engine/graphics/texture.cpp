#include "engine/graphics/texture.h"
#include <windows.h>
#include <wincodec.h>
#include <stdexcept>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace
{
	class ComApartment
	{
		public:
			ComApartment()
			{
				const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
				shouldUninitialize = result == S_OK || result == S_FALSE;

				if (FAILED(result) && result != RPC_E_CHANGED_MODE)
					throw std::runtime_error("Failed to initialize the Windows image loader");
			}

			~ComApartment()
			{
				if (shouldUninitialize)
					CoUninitialize();
			}

		private:
			bool shouldUninitialize = false;
	};
}

Texture Texture::LoadPng(const TextureId id, const std::filesystem::path& path, const bool firstAnimationFrame)
{
	ComApartment apartment;

	ComPtr<IWICImagingFactory> factory;
	if (FAILED(CoCreateInstance(
		CLSID_WICImagingFactory,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&factory)
	)))
		throw std::runtime_error("Failed to create the Windows image factory");

	ComPtr<IWICBitmapDecoder> decoder;
	if (FAILED(factory->CreateDecoderFromFilename(
		path.c_str(),
		nullptr,
		GENERIC_READ,
		WICDecodeMetadataCacheOnLoad,
		&decoder
	)))
		throw std::runtime_error("Failed to open PNG texture: " + path.string());

	ComPtr<IWICBitmapFrameDecode> frame;
	if (FAILED(decoder->GetFrame(0, &frame)))
		throw std::runtime_error("Failed to read PNG texture frame");

	UINT width = 0;
	UINT height = 0;
	if (FAILED(frame->GetSize(&width, &height)))
		throw std::runtime_error("Failed to read PNG texture dimensions");

	ComPtr<IWICFormatConverter> converter;
	if (FAILED(factory->CreateFormatConverter(&converter)))
		throw std::runtime_error("Failed to create PNG format converter");

	if (FAILED(converter->Initialize(
		frame.Get(),
		GUID_WICPixelFormat32bppRGBA,
		WICBitmapDitherTypeNone,
		nullptr,
		0.0,
		WICBitmapPaletteTypeCustom
	)))
		throw std::runtime_error("Failed to convert PNG texture to RGBA");

	// Minecraft animated block textures are vertical strips of square frames.
	if (firstAnimationFrame && height > width && height % width == 0) height = width;
	const WICRect region{.X = 0, .Y = 0, .Width = static_cast<INT>(width), .Height = static_cast<INT>(height)};
	std::vector<std::uint8_t> pixels(width * height * 4);
	if (FAILED(converter->CopyPixels(
		&region,
		width * 4,
		static_cast<UINT>(pixels.size()),
		pixels.data()
	)))
		throw std::runtime_error("Failed to copy PNG texture pixels");

	return Texture(id, width, height, std::move(pixels));
}
