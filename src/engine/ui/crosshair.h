#pragma once

#include "engine/ui/ui_element.h"
#include "engine/graphics/texture.h"

class Renderer;

class Crosshair : public UIElement
{
	public:
		Crosshair(Renderer& renderer, const std::filesystem::path& path);
		void Render() const override;

	private:
		Renderer& renderer;
		Texture image;
};
