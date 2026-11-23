#pragma once

#include "engine/ui/ui_element.h"
#include "simulation/world/block_registry.h"
#include "simulation/world/block_raycast.h"
#include <sstream>

class TargetOverlay : public UIElement
{
	public:
		TargetOverlay(Renderer& renderer, const Camera& camera, const World& world,
		              const BlockRegistry& blocks)
			: renderer(renderer), camera(camera), world(world), blocks(blocks) {}

		void Render() const override
		{
			const auto forward=Forward(camera.GetYaw(),camera.GetPitch());
			const auto p=camera.GetPosition();
			const auto hit = RaycastBlock(world, {.x = p.x,.y = p.y,.z = p.z}, forward,
				world.GetChunkRadius() * Chunk::Width - 2.0f);

			if (!hit) return;
			std::ostringstream text;
			text << blocks.GetBlockName(hit->block) << '\n'
				<< "X: " << hit->x << "  Y: " << hit->y << "  Z: " << hit->z;
			renderer.DrawTargetOverlay(text.str());
		}

	private:
		Renderer& renderer;
		const Camera& camera;
		const World& world;
		const BlockRegistry& blocks;
};
