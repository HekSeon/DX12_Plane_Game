#pragma once
#include <DirectXMath.h>

// Dalga ayarları küresel denizin yerel yukarı yönüne uygulanır.
// Apply wave settings along the spherical ocean's local up direction.
class Sea
{
  public:
    static constexpr float PrimaryAmplitude = 2.1f;
    static constexpr float SecondaryAmplitude = .95f;
    static constexpr float PrimaryFrequency = .025f;
    static constexpr float SecondaryFrequency = .04f;
    static constexpr float Speed = 1.1f;
    static constexpr float SecondarySpeed = .8f;
    static float Height(DirectX::FXMVECTOR surface, float time);
};
