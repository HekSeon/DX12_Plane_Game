#pragma once
#include <DirectXMath.h>
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>
class Game;
struct HudVertex
{
    DirectX::XMFLOAT2 position, uv;
    DirectX::XMFLOAT4 color;
};
struct HudAtlas
{
    static constexpr unsigned Width = 1024, Height = 384, Cell = 64;
    std::vector<unsigned char> pixels;
    std::array<float, 96> advance{};
};

// HUD geometrisi ekran pikseliyle kurulur; renderer yalnızca son katmanı çizer.
// Build HUD geometry in screen pixels; the renderer draws it as the final overlay.
class Hud
{
  public:
    static HudAtlas CreateFontAtlas();
    void Build(const Game &game, unsigned width, unsigned height, const HudAtlas &atlas);
    const std::vector<HudVertex> &Vertices() const { return m_vertices; }

  private:
    void Quad(float x, float y, float w, float h, DirectX::XMFLOAT4 color);
    void Triangle(DirectX::XMFLOAT2 a, DirectX::XMFLOAT2 b, DirectX::XMFLOAT2 c, DirectX::XMFLOAT4 color);
    void Line(float x0, float y0, float x1, float y1, float thickness, DirectX::XMFLOAT4 color);
    void Arc(float x, float y, float radius, float start, float end, float thickness, DirectX::XMFLOAT4 color);
    void Text(std::string_view text, float x, float y, float size, DirectX::XMFLOAT4 color, const HudAtlas &atlas,
              bool centered = false);
    void Panel(float x, float y, float w, float h, float radius, DirectX::XMFLOAT4 color);
    std::vector<HudVertex> m_vertices;
};
