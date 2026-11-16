#pragma once
#include "model/mesh.h"
#include <vector>
struct AirportPart
{
    MeshData mesh;
    DirectX::XMFLOAT4 color;
};
// Pist, taksi yolu, hangarlar ve kule için sabit sahne geometrisi üretir.
// Build static scene geometry for the runway, taxiway, hangars and tower.
class Airport
{
  public:
    static std::vector<AirportPart> Generate();
};
