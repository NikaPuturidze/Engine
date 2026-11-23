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


namespace
{
	class PngReader
	{
		public:
			explicit PngReader(const std::filesystem::path& path)
			{
				CreateFactory();
				OpenFirstFrame(path);
				ReadDimensions();
				ConvertToRgba();
			}

			Texture Read(const TextureId id, const bool firstAnimationFrame) const
			{
				const UINT width = imageWidth;
				UINT height = imageHeight;
				// Minecraft animated block textures are vertical strips of square frames.
				if (firstAnimationFrame && height > width && height % width == 0) height = width;
				const WICRect region{
					.X = 0, .Y = 0, .Width = static_cast<INT>(width), .Height = static_cast<INT>(height)
				};
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

		private:
			ComApartment apartment;
			ComPtr<IWICImagingFactory> factory;
			ComPtr<IWICBitmapDecoder> decoder;
			ComPtr<IWICBitmapFrameDecode> frame;
			ComPtr<IWICFormatConverter> converter;
			UINT imageWidth = 0, imageHeight = 0;

			void CreateFactory()
			{
				if (FAILED(CoCreateInstance(
					CLSID_WICImagingFactory,
					nullptr,
					CLSCTX_INPROC_SERVER,
					IID_PPV_ARGS(&factory)
				)))
					throw std::runtime_error("Failed to create the Windows image factory");
			}

			void OpenFirstFrame(const std::filesystem::path& path)
			{
				if (FAILED(factory->CreateDecoderFromFilename(
					path.c_str(),
					nullptr,
					GENERIC_READ,
					WICDecodeMetadataCacheOnLoad,
					&decoder
				)))
					throw std::runtime_error("Failed to open PNG texture: " + path.string());

				if (FAILED(decoder->GetFrame(0, &frame)))
					throw std::runtime_error("Failed to read PNG texture frame");
			}

			void ReadDimensions()
			{
				if (FAILED(frame->GetSize(&imageWidth, &imageHeight)))
					throw std::runtime_error("Failed to read PNG texture dimensions");
			}

			void ConvertToRgba()
			{
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
			}
	};
}

Texture Texture::LoadPng(const TextureId id, const std::filesystem::path& path, const bool firstAnimationFrame)
{
	return PngReader(path).Read(id, firstAnimationFrame);
}
