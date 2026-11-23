#include "sound_player.h"
#include <windows.h>
#include <xaudio2.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <list>
#include <random>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

struct SoundPlayer::State
{
	struct Clip
	{
		WAVEFORMATEX format{};
		std::vector<BYTE> samples;
	};
	IXAudio2* engine = nullptr;
	IXAudio2MasteringVoice* output = nullptr;
	std::list<IXAudio2SourceVoice*> voices;
	std::unordered_map<std::string, Clip> clips;
	std::unordered_map<std::string, std::vector<std::string>> events;
	std::mt19937 random{std::random_device{}()};
	bool uninitializeCom = false;

	~State()
	{
		for (auto* voice : voices) voice->DestroyVoice();
		if (output) output->DestroyVoice();
		if (engine) engine->Release();
		if (uninitializeCom) CoUninitialize();
	}

	const Clip& Load(const std::string& path)
	{
		if (const auto found = clips.find(path); found != clips.end()) return found->second;
		std::ifstream file(std::filesystem::path(path), std::ios::binary);
		const std::vector<char> bytes{std::istreambuf_iterator<char>(file), {}};
		if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) || std::memcmp(bytes.data() + 8, "WAVE", 4))
			throw std::runtime_error("Invalid WAV sound: " + path);
		Clip clip;
		for (std::size_t offset = 12; offset + 8 <= bytes.size();)
		{
			std::uint32_t size;
			std::memcpy(&size, bytes.data() + offset + 4, 4);
			const auto start = offset + 8;
			if (size > bytes.size() - start) throw std::runtime_error("Truncated WAV sound: " + path);
			if (!std::memcmp(bytes.data() + offset, "fmt ", 4) && size >= 16)
				std::memcpy(&clip.format, bytes.data() + start, 16);
			if (!std::memcmp(bytes.data() + offset, "data", 4))
				clip.samples.assign(bytes.begin() + start, bytes.begin() + start + size);
			offset = start + size + (size & 1);
		}
		if (clip.format.wFormatTag != WAVE_FORMAT_PCM || clip.format.wBitsPerSample != 16 ||
			clip.format.nChannels != 1 || clip.format.nSamplesPerSec != 44100 || clip.samples.empty())
			throw std::runtime_error("Expected imported mono 44100 Hz PCM sound: " + path);
		return clips.emplace(path, std::move(clip)).first->second;
	}
};

SoundPlayer::SoundPlayer() : state(std::make_unique<State>())
{
	const auto result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	state->uninitializeCom = result == S_OK || result == S_FALSE;
	if (FAILED(XAudio2Create(&state->engine, 0, XAUDIO2_DEFAULT_PROCESSOR)) ||
		FAILED(state->engine->CreateMasteringVoice(&state->output)))
		OutputDebugStringA("Audio output unavailable; continuing without sound.\n");
}

SoundPlayer::~SoundPlayer() = default;

void SoundPlayer::Update()
{
	state->voices.remove_if([](IXAudio2SourceVoice* voice)
	{
		XAUDIO2_VOICE_STATE status{};
		voice->GetState(&status);
		if (status.BuffersQueued != 0) return false;
		voice->DestroyVoice();
		return true;
	});
}

void SoundPlayer::LoadEvents(const std::filesystem::path& namespaceDirectory)
{
	std::ifstream manifest(namespaceDirectory / "engine/player_sounds.tsv");
	std::string line;
	if (!std::getline(manifest, line) || line != "engine-sound-events-v1")
		throw std::runtime_error("Missing or invalid player sound manifest: " + (namespaceDirectory / "engine/player_sounds.tsv").string());
	while (std::getline(manifest, line))
	{
		const auto separator = line.find('\t');
		if (separator == std::string::npos) throw std::runtime_error("Invalid sound event row");
		std::vector<std::string> variants;
		std::istringstream paths(line.substr(separator + 1));
		std::string path;
		while (std::getline(paths, path, ',')) variants.push_back((namespaceDirectory / path).string());
		if (variants.empty()) throw std::runtime_error("Empty sound event: " + line);
		state->events[line.substr(0, separator)] = std::move(variants);
	}
}

void SoundPlayer::PlayEvent(std::string_view event, float volume, float pitch)
{
	const auto found = state->events.find(std::string(event));
	if (found == state->events.end()) throw std::runtime_error("Unknown sound event: " + std::string(event));
	Play(found->second, volume, pitch);
}

void SoundPlayer::Play(const std::vector<std::string>& variants, float volume, float pitch)
{
	if (!state->output || variants.empty()) return;
	Update();
	if (state->voices.size() >= 32) return;
	const auto index = std::uniform_int_distribution<std::size_t>(0, variants.size() - 1)(state->random);
	const auto& clip = state->Load(variants[index]);
	IXAudio2SourceVoice* voice = nullptr;
	if (FAILED(state->engine->CreateSourceVoice(&voice, &clip.format)))
		throw std::runtime_error("Failed to create audio voice");
	const XAUDIO2_BUFFER buffer{.Flags = XAUDIO2_END_OF_STREAM,
		.AudioBytes = static_cast<UINT32>(clip.samples.size()), .pAudioData = clip.samples.data()};
	if (FAILED(voice->SubmitSourceBuffer(&buffer)) || FAILED(voice->SetVolume((std::clamp)(volume, 0.0f, 1.0f))) ||
		FAILED(voice->SetFrequencyRatio((std::clamp)(pitch, 0.1f, 2.0f))) ||
		FAILED(voice->Start()))
	{
		voice->DestroyVoice();
		throw std::runtime_error("Failed to play block sound");
	}
	state->voices.push_back(voice);
}
