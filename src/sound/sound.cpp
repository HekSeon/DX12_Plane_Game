#include "sound.h"
#include <Windows.h>
#include <xaudio2.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <vector>
using Microsoft::WRL::ComPtr;
namespace
{
struct Wave
{
    WAVEFORMATEX format{};
    std::vector<BYTE> data;
};
uint32_t Read32(const BYTE *p)
{
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
uint16_t Read16(const BYTE *p)
{
    return uint16_t(p[0]) | uint16_t(p[1]) << 8;
}
Wave ReadWave(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    auto size = file.tellg();
    if (!file || size < 12 || size > 64 * 1024 * 1024)
        throw std::runtime_error("Invalid WAV size");
    std::vector<BYTE> bytes(static_cast<size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char *>(bytes.data()), size);
    if (!file || std::memcmp(bytes.data(), "RIFF", 4) || std::memcmp(bytes.data() + 8, "WAVE", 4))
        throw std::runtime_error("Expected RIFF WAVE");
    size_t end = size_t(Read32(bytes.data() + 4)) + 8;
    if (end > bytes.size())
        throw std::runtime_error("Truncated WAV");
    Wave wave;
    bool formatFound = false;
    // RIFF parçaları farklı sırada gelebilir; tek baytlık dolgu da hesaba katılır.
    // RIFF chunks may arrive in any order; account for their byte padding.
    for (size_t p = 12; p + 8 <= end;)
    {
        uint32_t count = Read32(bytes.data() + p + 4);
        size_t start = p + 8;
        if (count > end - start)
            throw std::runtime_error("Truncated WAV chunk");
        if (!std::memcmp(bytes.data() + p, "fmt ", 4))
        {
            if (count < 16)
                throw std::runtime_error("Invalid WAV format");
            auto f = bytes.data() + start;
            auto &format = wave.format;
            format.wFormatTag = Read16(f);
            format.nChannels = Read16(f + 2);
            format.nSamplesPerSec = Read32(f + 4);
            format.nAvgBytesPerSec = Read32(f + 8);
            format.nBlockAlign = Read16(f + 12);
            format.wBitsPerSample = Read16(f + 14);
            if (format.wFormatTag != WAVE_FORMAT_PCM || format.wBitsPerSample != 16 || !format.nChannels ||
                format.nChannels > 2 || format.nSamplesPerSec < 8000 || format.nSamplesPerSec > 192000 ||
                format.nBlockAlign != format.nChannels * 2)
                throw std::runtime_error("Use 16-bit PCM mono/stereo WAV");
            formatFound = true;
        }
        else if (!std::memcmp(bytes.data() + p, "data", 4))
            wave.data.assign(bytes.begin() + start, bytes.begin() + start + count);
        p = start + count + (count & 1);
    }
    if (!formatFound || wave.data.empty() || wave.data.size() % wave.format.nBlockAlign)
        throw std::runtime_error("Missing/invalid WAV data");
    return wave;
}
// Motor kaydı yoksa düşük sesli bir pervane tonu üret.
// Generate a quiet propeller tone when no engine recording is available.
Wave EngineTone()
{
    Wave wave;
    wave.format = {WAVE_FORMAT_PCM, 1, 22050, 44100, 2, 16, 0};
    constexpr int samples = 44100;
    wave.data.resize(samples * 2);
    for (int i = 0; i < samples; ++i)
    {
        float t = float(i) / 22050.f;
        float pulse = std::sin(2 * std::numbers::pi_v<float> * 55 * t) +
                      .4f * std::sin(2 * std::numbers::pi_v<float> * 110 * t) +
                      .15f * std::sin(2 * std::numbers::pi_v<float> * 165 * t);
        int16_t sample = int16_t(pulse * 7000);
        std::memcpy(wave.data.data() + i * 2, &sample, 2);
    }
    return wave;
}
} // namespace
struct Sound::Impl
{
    struct Channel
    {
        Wave wave;
        IXAudio2SourceVoice *voice = nullptr;
    };
    ComPtr<IXAudio2> audio;
    IXAudio2MasteringVoice *master = nullptr;
    std::array<Channel, 3> channels;
    bool comOwned = false, active = false;
    ~Impl()
    {
        for (auto &channel : channels)
            if (channel.voice)
                channel.voice->DestroyVoice();
        if (master)
            master->DestroyVoice();
        audio.Reset();
        if (comOwned)
            CoUninitialize();
    }
    void Load(int slot, const std::filesystem::path &path, bool fallback)
    {
        auto &channel = channels[slot];
        try
        {
            if (std::filesystem::exists(path))
                channel.wave = ReadWave(path);
            else if (fallback)
                channel.wave = EngineTone();
            else
                return;
        }
        catch (const std::exception &error)
        {
            std::cerr << path.string() << ": " << error.what() << '\n';
            if (fallback)
                channel.wave = EngineTone();
            else
                return;
        }
        if (FAILED(audio->CreateSourceVoice(&channel.voice, &channel.wave.format, 0, 2.f)))
            return;
        XAUDIO2_BUFFER buffer{};
        buffer.AudioBytes = UINT32(channel.wave.data.size());
        buffer.pAudioData = channel.wave.data.data();
        buffer.Flags = XAUDIO2_END_OF_STREAM;
        buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
        if (FAILED(channel.voice->SubmitSourceBuffer(&buffer)))
        {
            channel.voice->DestroyVoice();
            channel.voice = nullptr;
            return;
        }
        channel.voice->SetVolume(0);
        channel.voice->Start();
    }
};
Sound::Sound() = default;
Sound::~Sound() = default;
bool Sound::Initialize()
{
    auto sound = std::make_unique<Impl>();
    HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    sound->comOwned = SUCCEEDED(com);
    if (FAILED(XAudio2Create(&sound->audio, 0)) || FAILED(sound->audio->CreateMasteringVoice(&sound->master)))
        return false;
    sound->Load(0, "data/SE/engine.wav", true);
    sound->Load(1, "data/SE/wind.wav", false);
    sound->Load(2, "data/BGM/flight.wav", false);
    m_impl = std::move(sound);
    return true;
}
void Sound::Update(float throttle, float speed, bool active)
{
    if (!m_impl)
        return;
    throttle = std::clamp(throttle, 0.f, 1.f);
    if (m_impl->active != active)
    {
        for (auto &c : m_impl->channels)
            if (c.voice)
            {
                if (active)
                    c.voice->Start();
                else
                    c.voice->Stop();
            }
        m_impl->active = active;
    }
    if (auto voice = m_impl->channels[0].voice)
    {
        voice->SetFrequencyRatio(.65f + throttle * .9f);
        voice->SetVolume(active && throttle > 0 ? .08f + throttle * .16f : 0);
    }
    if (auto voice = m_impl->channels[1].voice)
        voice->SetVolume(active ? std::clamp(speed / 230.f, 0.f, 1.f) * .2f : 0);
    if (auto voice = m_impl->channels[2].voice)
        voice->SetVolume(active ? .12f : 0);
}
void Sound::Shutdown()
{
    m_impl.reset();
}
