#pragma once
#include <Windows.h>
#include <cstdint>

class Game;

class IRenderer
{
  public:
    virtual ~IRenderer() = default;
    virtual bool Initialize(HWND window, uint32_t width, uint32_t height, const Game &game) = 0;
    virtual void Resize(uint32_t width, uint32_t height) = 0;
    virtual void Render() = 0;
    virtual void Shutdown() = 0;
};
