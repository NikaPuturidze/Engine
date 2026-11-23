#pragma once

#include <memory>
#include <filesystem>
#include <string_view>
#include <string>
#include <vector>

class SoundPlayer
{
public:
	SoundPlayer();
	~SoundPlayer();
	SoundPlayer(const SoundPlayer&) = delete;
	SoundPlayer& operator=(const SoundPlayer&) = delete;
	void Update();
	void LoadEvents(const std::filesystem::path& namespaceDirectory);
	void PlayEvent(std::string_view event, float volume = 1.0f, float pitch = 1.0f);
	void Play(const std::vector<std::string>& variants, float volume = 1.0f, float pitch = 1.0f);

private:
	struct State;
	std::unique_ptr<State> state;
};
