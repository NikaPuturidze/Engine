#pragma once

#include <cstdint>
#include <filesystem>
#include <utility>
#include <vector>

using TextureId = std::uint64_t;

class Texture
{
public:
	Texture(const TextureId id, const std::uint32_t width, const std::uint32_t height, std::vector<std::uint8_t> pixels)
		: id(id), width(width), height(height), pixels(std::move(pixels)) {}

	static Texture LoadPng(TextureId id, const std::filesystem::path& path);

	const TextureId id;
	const std::uint32_t width;
	const std::uint32_t height;
	const std::vector<std::uint8_t> pixels;
};
