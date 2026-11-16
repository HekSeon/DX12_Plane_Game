#pragma once
#include <filesystem>
#include <memory>

// Ses aygıtı kullanılamıyorsa oyun sessiz devam eder.
// Keep the game running silently if the audio device is unavailable.
class Sound
{
  public:
    Sound();
    ~Sound();
    bool Initialize();
    void Update(float throttle, float speed, bool active);
    void Shutdown();

  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
