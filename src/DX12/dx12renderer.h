#pragma once
#include "IRenderer.h"
#include <memory>

class DX12Renderer final : public IRenderer
{
  public:
    DX12Renderer();
    ~DX12Renderer() override;
    bool Initialize(HWND window, uint32_t width, uint32_t height, const Game &game) override;
    void Resize(uint32_t width, uint32_t height) override;
    void Render() override;
    void Shutdown() override;

  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
