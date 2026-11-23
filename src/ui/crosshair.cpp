#include "crosshair.h"
#include "../graphics/renderer.h"

Crosshair::Crosshair(Renderer& renderer, const std::filesystem::path& path)
	: renderer(renderer), image(Texture::LoadPng(0, path))
{
}

void Crosshair::Render() const
{
	renderer.DrawCrosshair(image);
}
