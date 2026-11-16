#pragma once
#include <array>
#include <filesystem>
#include <vector>

struct ModelVertex
{
    std::array<float, 3> position{}, normal{};
    std::array<float, 2> uv{};
};

class Model
{
  public:
    // Dışbükey OBJ yüzlerini üçgenleştirir ve DirectX eksen düzenine dönüştürür.
    // Triangulate convex OBJ faces and convert to DirectX coordinates.
    static std::vector<ModelVertex> Load(const std::filesystem::path &path);
};
