#include "sea.h"
#include <cmath>
using namespace DirectX;
float Sea::Height(FXMVECTOR surface, float time)
{
    auto first = XMVector3Normalize(XMVectorSet(1, .2f, .7f, 0));
    auto second = XMVector3Normalize(XMVectorSet(-.4f, .3f, 1, 0));
    return std::sin(XMVectorGetX(XMVector3Dot(surface, first)) * PrimaryFrequency - time * Speed) * PrimaryAmplitude +
           std::sin(XMVectorGetX(XMVector3Dot(surface, second)) * SecondaryFrequency + time * SecondarySpeed) *
               SecondaryAmplitude;
}
