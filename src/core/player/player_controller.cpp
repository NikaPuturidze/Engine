#include "player_controller.h"
#include "../../world/block_raycast.h"
#include "../../world/block_registry.h"
#include "../../audio/sound_player.h"
#include <commctrl.h>
#include <cmath>
#include <stdexcept>

PlayerController::PlayerController(
	Window& window, Player& player, Camera& camera) :
	window(window), player(player), camera(camera), previousPosition(player.GetPosition())
{
	if (!SetWindowSubclass(window.GetHandle(), SubClassProc, 1,
	                       reinterpret_cast<DWORD_PTR>(this)))
		throw std::runtime_error("Failed to install player input handler");

	UpdateCursor(GetFocus() == window.GetHandle());

	const auto position = player.GetPosition();
	camera.SetPosition(position.x, position.y + Player::EyeHeight, position.z);
}

PlayerController::~PlayerController()
{
	RemoveWindowSubclass(window.GetHandle(), SubClassProc, 1);
	UpdateCursor(false);
}

void PlayerController::Spawn(const World& world)
{
	player.Spawn(world);
	previousPosition = player.GetPosition();
	horizontalSpeed = 0.0f;
	footsteps.Reset();
	UpdateCamera(1.0f);
}

void PlayerController::UpdateCursor(const bool focused)
{
	const HWND hwnd = window.GetHandle();
	RECT bounds{};

	if (focused && !IsIconic(hwnd) && GetClientRect(hwnd, &bounds) &&
		bounds.right > bounds.left && bounds.bottom > bounds.top)
	{
		POINT origin{.x = bounds.left, .y = bounds.top};
		POINT end{.x = bounds.right, .y = bounds.bottom};
		ClientToScreen(hwnd, &origin);
		ClientToScreen(hwnd, &end);
		bounds = {.left = origin.x, .top = origin.y, .right = end.x, .bottom = end.y};
		ClipCursor(&bounds);
		if (!cursorHidden)
		{
			ShowCursor(FALSE);
			cursorHidden = true;
		}
		SetCursor(nullptr);
	}
	else if (cursorHidden)
	{
		ClipCursor(nullptr);
		ShowCursor(TRUE);
		SetCursor(LoadCursor(nullptr, IDC_ARROW));
		cursorHidden = false;
	}
}

void PlayerController::Update(const float tickDuration, const World& world)
{
	previousPosition = player.GetPosition();
	UpdateMovement(tickDuration);
	if (jumping || jumpRequested) player.Jump();
	jumpRequested = false;
	player.Update(tickDuration, world);
	const auto position = player.GetPosition();
	const float dx = position.x - previousPosition.x;
	const float dz = position.z - previousPosition.z;
	horizontalSpeed = tickDuration > 0.0f ? std::hypot(dx, dz) / tickDuration : 0.0f;
}

float PlayerController::GetYaw() const
{
	const float yaw = std::fmod(camera.GetYaw(), 360.0f);
	return yaw < 0.0f ? yaw + 360.0f : yaw;
}

void PlayerController::UpdateCamera(const float alpha) const
{
	const auto position = player.GetPosition();
	camera.SetPosition(
		previousPosition.x + (position.x - previousPosition.x) * alpha,
		previousPosition.y + (position.y - previousPosition.y) * alpha + Player::EyeHeight,
		previousPosition.z + (position.z - previousPosition.z) * alpha
	);
}

void PlayerController::UpdateLook() const
{
	long x = 0;
	long y = 0;
	window.ConsumeMouseMovement(x, y);
	constexpr float sensitivity = 0.1f;
	camera.Rotate(x * sensitivity, y * sensitivity);
}

void PlayerController::UpdateLandingSounds(const World& world, const BlockRegistry& blocks, SoundPlayer& sounds)
{
	const auto fall = player.ConsumeLandingSound();
	const auto block = player.ConsumeLandingBlock();
	if (fall == FallSound::None) return;
	sounds.PlayEvent(fall == FallSound::Big ? "entity.player.big_fall" : "entity.player.small_fall");
	if (block == 0) return;
	sounds.Play(blocks.GetBlockDefinition(block)->fallingSounds, 0.5f, 0.75f);
}

void PlayerController::UpdateFootsteps(float deltaSeconds, const World& world,
	const BlockRegistry& blocks, SoundPlayer& sounds)
{
	if (!footsteps.Advance(horizontalSpeed * deltaSeconds, player.IsGrounded())) return;
	const auto position = player.GetPosition();
	const auto id = world.GetBlock(static_cast<int>(std::floor(position.x)),
		static_cast<int>(std::floor(position.y - 0.05f)), static_cast<int>(std::floor(position.z)));
	if (id != 0) sounds.Play(blocks.GetBlockDefinition(id)->movementSounds, 0.15f);
}

void PlayerController::UpdateInteraction(World& world, const BlockRegistry& blocks,
	SoundPlayer& sounds, const BlockId placementBlock, const float deltaSeconds)
{
	const auto mode = player.GetGameMode();
	if (mode != previousGameMode)
	{
		breakingBlock.reset();
		hitSoundTime = 0.0f;
		creativeBreakCooldown = 0.0f;
		previousGameMode = mode;
	}
	creativeBreakCooldown = (std::max)(0.0f, creativeBreakCooldown - deltaSeconds);
	const bool creative = mode == GameMode::Creative;
	// First click is immediate; held Creative mining repeats at a bounded rate.
	const bool remove = removeBlockRequested || (breakingHeld && (!creative || creativeBreakCooldown <= 0.0f));
	const bool place = placeBlockRequested;
	removeBlockRequested = false;
	placeBlockRequested = false;
	if (!remove && !place)
	{
		breakingBlock.reset();
		return;
	}
	DirectX::XMFLOAT3 forward;
	const auto inverseView = DirectX::XMMatrixInverse(nullptr, camera.GetViewMatrix());
	DirectX::XMStoreFloat3(&forward, DirectX::XMVector3TransformNormal(
		DirectX::XMVectorSet(0, 0, 1, 0), inverseView));
	if (const auto hit = RaycastBlock(world, camera.GetPosition(), forward,
		6.0f))
	{
		const BlockPosition position{hit->x, hit->y, hit->z};
		if (remove)
		{
			if (!breakingBlock || breakingBlock->GetId() != hit->block || breakingBlock->GetPosition() != position)
			{
				breakingBlock.emplace(blocks.CreateBlock(hit->block, position));
				hitSoundTime = 0.0f;
			}
			if (creative || breakingBlock->IsDestructible())
			{
				hitSoundTime -= deltaSeconds;
				if (!creative && hitSoundTime <= 0.0f)
				{
					sounds.Play(breakingBlock->GetDefinition().hittingSounds, 0.25f, 0.5f);
					hitSoundTime += 0.2f;
				}
				if (breakingBlock->UpdateMining(mode, deltaSeconds, player.IsGrounded()))
				{
					if (world.SetBlock(hit->x, hit->y, hit->z, 0))
						sounds.Play(breakingBlock->GetDefinition().breakingSounds, 1.0f, 0.8f);
					breakingBlock.reset();
					if (creative) creativeBreakCooldown = 0.2f;
					return;
				}
			}
		}
		else breakingBlock.reset();
		if (place && (hit->normalX || hit->normalY || hit->normalZ))
			if (player.PlaceBlock(world, hit->x + hit->normalX, hit->y + hit->normalY,
				hit->z + hit->normalZ, placementBlock))
				sounds.Play(blocks.GetBlockDefinition(placementBlock)->placingSounds, 1.0f, 0.8f);
	}
	else breakingBlock.reset();
}

void PlayerController::UpdateMovement(const float tickDuration) const
{
	player.Move(
		static_cast<float>(movingRight) - static_cast<float>(movingLeft),
		static_cast<float>(movingForward) - static_cast<float>(movingBackward),
		camera.GetYaw(),
		tickDuration,
		sprintInput.IsSprinting() && !movingBackward
	);
}

LRESULT CALLBACK PlayerController::SubClassProc(
	const HWND hwnd,
	const UINT uMsg,
	const WPARAM wParam,
	const LPARAM lParam,
	const UINT_PTR uIdSubclass,
	const DWORD_PTR dwRefData)
{
	auto* controller = reinterpret_cast<PlayerController*>(dwRefData);

	switch (uMsg)
	{
		case WM_RBUTTONDOWN:
			if (GetFocus() == hwnd && controller->cursorHidden)
				controller->placeBlockRequested = true;
			break;
		case WM_LBUTTONDOWN:
			if (GetFocus() == hwnd && controller->cursorHidden)
			{
				controller->removeBlockRequested = true;
				controller->breakingHeld = true;
			}
			break;
		case WM_LBUTTONUP:
			controller->breakingHeld = false;
			break;
		case WM_SETFOCUS:
			controller->UpdateCursor(true);
			break;
		case WM_MOVE:
		case WM_SIZE:
		case WM_EXITSIZEMOVE:
			controller->UpdateCursor(GetFocus() == hwnd);
			break;
		case WM_ENTERSIZEMOVE:
			controller->UpdateCursor(false);
			break;
		case WM_SETCURSOR:
			if (controller->cursorHidden && LOWORD(lParam) == HTCLIENT)
			{
				SetCursor(nullptr);
				return TRUE;
			}
			break;
		case WM_KEYDOWN:
		case WM_KEYUP:
			{
				const bool pressed = uMsg == WM_KEYDOWN;
				if (wParam == VK_F4 && pressed && !(lParam & (1LL << 30)))
					controller->player.SetGameMode(controller->GetGameMode() == GameMode::Survival
						? GameMode::Creative : GameMode::Survival);
				if (wParam == 'W')
				{
					controller->movingForward = pressed;
					controller->sprintInput.SetForward(pressed);
				}
				if (wParam == 'S')
				{
					controller->movingBackward = pressed;
					if (pressed) controller->sprintInput.Cancel();
				}
				if (wParam == 'A') controller->movingLeft = pressed;
				if (wParam == 'D') controller->movingRight = pressed;
				if (wParam == VK_SPACE)
				{
					if (pressed && !controller->jumping) controller->jumpRequested = true;
					controller->jumping = pressed;
				}
				break;
			}
		case WM_KILLFOCUS:
			controller->sprintInput.Reset();
			controller->removeBlockRequested = false;
			controller->breakingHeld = false;
			controller->breakingBlock.reset();
			controller->placeBlockRequested = false;
			controller->jumping = false;
			controller->jumpRequested = false;
			controller->UpdateCursor(false);
			controller->movingForward = false;
			controller->movingBackward = false;
			controller->movingLeft = false;
			controller->movingRight = false;
			break;
		case WM_NCDESTROY:
			controller->UpdateCursor(false);
			RemoveWindowSubclass(hwnd, SubClassProc, uIdSubclass);
			break;
	}
	return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}
