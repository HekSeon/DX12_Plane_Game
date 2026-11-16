#include "hud.h"
#include "game/game.h"
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
using namespace DirectX;
HudAtlas Hud::CreateFontAtlas()
{
    // Yazılar ekran yakalamadan, yalnızca bellekteki bir bitmap üzerine çizilir.
    // Rasterize text into a memory bitmap only; no screen capture is involved.
    HudAtlas atlas;
    atlas.pixels.resize(size_t(HudAtlas::Width) * HudAtlas::Height * 4);
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc)
        throw std::runtime_error("HUD font DC creation failed");
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = HudAtlas::Width;
    info.bmiHeader.biHeight = -int(HudAtlas::Height);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    HFONT font = CreateFontW(-48, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    if (!bitmap || !font)
    {
        if (bitmap)
            DeleteObject(bitmap);
        if (font)
            DeleteObject(font);
        DeleteDC(dc);
        throw std::runtime_error("HUD font atlas creation failed");
    }
    auto oldBitmap = SelectObject(dc, bitmap), oldFont = SelectObject(dc, font);
    std::memset(bits, 0, atlas.pixels.size());
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    for (int i = 0; i < 96; ++i)
    {
        wchar_t ch = wchar_t(i + 32);
        SIZE size{};
        GetTextExtentPoint32W(dc, &ch, 1, &size);
        atlas.advance[i] = float(size.cx);
        TextOutW(dc, (i % 16) * 64, (i / 16) * 64, &ch, 1);
    }
    GdiFlush();
    auto src = static_cast<const unsigned char *>(bits);
    for (size_t i = 0; i < atlas.pixels.size(); i += 4)
    {
        atlas.pixels[i] = atlas.pixels[i + 1] = atlas.pixels[i + 2] = 255;
        atlas.pixels[i + 3] = std::max({src[i], src[i + 1], src[i + 2]});
    }
    // Son hücre düz renkli şekillerin beyaz örnek noktasıdır.
    // Reserve the last cell's center as a white sample for solid shapes.
    for (unsigned y = 352; y < 360; ++y)
        for (unsigned x = 992; x < 1000; ++x)
            atlas.pixels[(size_t(y) * 1024 + x) * 4 + 3] = 255;
    SelectObject(dc, oldFont);
    SelectObject(dc, oldBitmap);
    DeleteObject(font);
    DeleteObject(bitmap);
    DeleteDC(dc);
    return atlas;
}
void Hud::Triangle(XMFLOAT2 a, XMFLOAT2 b, XMFLOAT2 c, XMFLOAT4 color)
{
    XMFLOAT2 uv{996.f / 1024, 356.f / 384};
    m_vertices.insert(m_vertices.end(), {{a, uv, color}, {b, uv, color}, {c, uv, color}});
}
void Hud::Quad(float x, float y, float w, float h, XMFLOAT4 color)
{
    Triangle({x, y}, {x + w, y}, {x + w, y + h}, color);
    Triangle({x, y}, {x + w, y + h}, {x, y + h}, color);
}
void Hud::Line(float x0, float y0, float x1, float y1, float thickness, XMFLOAT4 color)
{
    float dx = x1 - x0, dy = y1 - y0, len = std::hypot(dx, dy);
    if (len < .001f)
        return;
    float nx = -dy / len * thickness * .5f, ny = dx / len * thickness * .5f;
    Triangle({x0 + nx, y0 + ny}, {x1 + nx, y1 + ny}, {x1 - nx, y1 - ny}, color);
    Triangle({x0 + nx, y0 + ny}, {x1 - nx, y1 - ny}, {x0 - nx, y0 - ny}, color);
}
void Hud::Arc(float x, float y, float radius, float start, float end, float thickness, XMFLOAT4 color)
{
    int steps = std::max(1, int(std::abs(end - start) * radius / 3));
    for (int i = 0; i < steps; ++i)
    {
        float a = std::lerp(start, end, float(i) / steps), b = std::lerp(start, end, float(i + 1) / steps);
        Line(x + std::cos(a) * radius, y + std::sin(a) * radius, x + std::cos(b) * radius, y + std::sin(b) * radius,
             thickness, color);
    }
}
void Hud::Panel(float x, float y, float w, float h, float r, XMFLOAT4 color)
{
    Quad(x + r, y, w - 2 * r, h, color);
    Quad(x, y + r, r, h - 2 * r, color);
    Quad(x + w - r, y + r, r, h - 2 * r, color);
    for (int c = 0; c < 4; ++c)
    {
        float cx = c % 2 ? x + w - r : x + r, cy = c / 2 ? y + h - r : y + r;
        float start = c == 0 ? XM_PI : c == 1 ? -XM_PIDIV2 : c == 2 ? XM_PIDIV2 : 0;
        for (int i = 0; i < 12; ++i)
        {
            float a = start + XM_PIDIV2 * i / 12, b = start + XM_PIDIV2 * (i + 1) / 12;
            Triangle({cx, cy}, {cx + r * std::cos(a), cy + r * std::sin(a)},
                     {cx + r * std::cos(b), cy + r * std::sin(b)}, color);
        }
    }
}
void Hud::Text(std::string_view text, float x, float y, float size, XMFLOAT4 color, const HudAtlas &atlas,
               bool centered)
{
    float scale = size / 48.f, total = 0;
    for (unsigned char ch : text)
        if (ch >= 32 && ch < 127)
            total += atlas.advance[ch - 32] * scale;
    if (centered)
        x -= total * .5f;
    for (unsigned char ch : text)
    {
        if (ch < 32 || ch >= 127)
            continue;
        int index = ch - 32;
        float glyphWidth = std::min(63.f, atlas.advance[index] + 2.f);
        float u = (float(index % 16) * 64 + .5f) / 1024, v = (float(index / 16) * 64 + .5f) / 384,
              w = glyphWidth * scale, h = 64 * scale;
        float du = (glyphWidth - 1) / 1024, dv = 63.f / 384;
        HudVertex a{{x, y}, {u, v}, color}, b{{x + w, y}, {u + du, v}, color},
            c{{x + w, y + h}, {u + du, v + dv}, color}, d{{x, y + h}, {u, v + dv}, color};
        m_vertices.insert(m_vertices.end(), {a, b, c, a, c, d});
        x += atlas.advance[index] * scale;
    }
}
void Hud::Build(const Game &game, unsigned width, unsigned height, const HudAtlas &atlas)
{
    m_vertices.clear();
    m_vertices.reserve(14000);
    float s = std::clamp(std::min(width / 1680.f, height / 920.f), .45f, 2.5f);
    float margin = 32 * s;
    XMFLOAT4 white{1, 1, 1, .96f}, faint{1, 1, 1, .4f}, shadow{.015f, .02f, .03f, .48f};
    const auto &player = game.GetPlayer();
    char buffer[96];
    Panel(margin, margin, 178 * s, 153 * s, 8 * s, shadow);
    Text("ALTITUDE", margin + 13 * s, margin + 8 * s, 18 * s, white, atlas);
    float mx = margin + 18 * s, my = margin + 62 * s;
    Triangle({mx, my}, {mx + 9 * s, my - 20 * s}, {mx + 19 * s, my}, white);
    Triangle({mx + 12 * s, my}, {mx + 23 * s, my - 14 * s}, {mx + 32 * s, my}, faint);
    std::snprintf(buffer, sizeof(buffer), "%.0f m", player.Altitude());
    Text(buffer, margin + 63 * s, margin + 43 * s, 24 * s, white, atlas);
    Line(margin + 12 * s, margin + 77 * s, margin + 166 * s, margin + 77 * s, 1 * s, faint);
    Text("SPEED", margin + 13 * s, margin + 81 * s, 18 * s, white, atlas);
    float gx = margin + 29 * s, gy = margin + 129 * s;
    Arc(gx, gy, 12 * s, .7f, 5.58f, 2.5f * s, white);
    Line(gx, gy, gx + 8 * s, gy - 8 * s, 2 * s, white);
    std::snprintf(buffer, sizeof(buffer), "%.0f km/h", player.Speed() * 3.6f);
    Text(buffer, margin + 61 * s, margin + 110 * s, 24 * s, white, atlas);
    float center = width * .5f;
    int minutes = int(game.TimeOfDay().Hour() * 60);
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes / 60, minutes % 60);
    Text(buffer, center, 14 * s, 44 * s, white, atlas, true);
    Line(center - 280 * s, 41 * s, center - 90 * s, 41 * s, 2 * s, faint);
    Line(center + 90 * s, 41 * s, center + 280 * s, 41 * s, 2 * s, faint);
    using namespace std::chrono;
    auto date = year_month_day{sys_days{year{2026} / November / 16} + days{game.TimeOfDay().DaysElapsed()}};
    constexpr const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    std::snprintf(buffer, sizeof(buffer), "%s %02u, %d", months[unsigned(date.month()) - 1], unsigned(date.day()),
                  int(date.year()));
    Text(buffer, center, 66 * s, 17 * s, white, atlas, true);
    float cy = height * .48f;
    Line(center - 25 * s, cy, center - 13 * s, cy, 1 * s, white);
    Line(center + 13 * s, cy, center + 25 * s, cy, 1 * s, white);
    Line(center, cy - 25 * s, center, cy - 13 * s, 1 * s, white);
    Line(center, cy + 13 * s, center, cy + 25 * s, 1 * s, white);
    float radius = 94 * s, cx = width - margin - radius - 12 * s, ty = height - margin - radius;
    constexpr float start = XM_PI * .75f, sweep = XM_PI * 1.5f;
    Arc(cx, ty, radius + 12 * s, start, start + sweep, 1 * s, faint);
    Arc(cx, ty, radius, start, start + sweep, 8 * s, {1, 1, 1, .2f});
    Arc(cx, ty, radius, start, start + sweep * player.Throttle(), 8 * s, white);
    for (int i = 0; i <= 20; ++i)
    {
        float angle = start + sweep * i / 20, r0 = radius + 10 * s, r1 = r0 + (i % 5 == 0 ? 9.f : 4.f) * s;
        Line(cx + std::cos(angle) * r0, ty + std::sin(angle) * r0, cx + std::cos(angle) * r1, ty + std::sin(angle) * r1,
             1 * s, white);
        if (i % 5 == 0)
        {
            std::snprintf(buffer, sizeof(buffer), "%d", i * 5);
            Text(buffer, cx + std::cos(angle) * (radius + 25 * s), ty + std::sin(angle) * (radius + 25 * s) - 8 * s,
                 14 * s, white, atlas, true);
        }
    }
    std::snprintf(buffer, sizeof(buffer), "%.0f%%", player.Throttle() * 100);
    Text(buffer, cx, ty - 36 * s, 42 * s, white, atlas, true);
    Text("THROTTLE", cx, ty + 12 * s, 15 * s, white, atlas, true);
}
