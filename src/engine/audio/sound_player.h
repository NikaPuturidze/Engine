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
	void Update() const;
	void LoadEvents(const std::filesystem::path& namespaceDirectory) const;
	void PlayEvent(std::string_view event, float volume = 1.0f, float pitch = 1.0f) const;
	void Play(const std::vector<std::string>& variants, float volume = 1.0f, float pitch = 1.0f) const;

private:
	struct State;
	std::unique_ptr<State> state;
};
