#pragma once

#include "ui_element.h"
#include "../world/block_registry.h"
#include "../world/block_raycast.h"
#include <sstream>

class TargetOverlay : public UIElement
{
	public:
		TargetOverlay(Renderer& renderer, const Camera& camera, const World& world,
		              const BlockRegistry& blocks)
			: renderer(renderer), camera(camera), world(world), blocks(blocks) {}

		void Render() const override
		{
			DirectX::XMFLOAT3 forward;
			const auto inverseView = DirectX::XMMatrixInverse(nullptr, camera.GetViewMatrix());
			DirectX::XMStoreFloat3(&forward, DirectX::XMVector3TransformNormal(
				DirectX::XMVectorSet(0, 0, 1, 0), inverseView));
			const auto hit = RaycastBlock(world, camera.GetPosition(), forward,
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
