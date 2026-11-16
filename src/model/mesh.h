#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include <vector>

struct Vertex
{
    DirectX::XMFLOAT3 position, normal;
    DirectX::XMFLOAT2 uv;
};

struct MeshData
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};
