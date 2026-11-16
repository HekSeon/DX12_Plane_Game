#include "model.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

std::vector<ModelVertex> Model::Load(const std::filesystem::path &path)
{
    std::ifstream file(path);
    if (!file)
        throw std::runtime_error("Cannot open OBJ: " + path.string());
    std::vector<std::array<float, 3>> positions, normals;
    std::vector<std::array<float, 2>> uvs;
    std::vector<ModelVertex> output;
    size_t lineNumber = 0;
    auto index = [](const std::string &text, size_t count) -> size_t {
        size_t used = 0;
        int value = std::stoi(text, &used);
        if (used != text.size() || !value)
            throw std::runtime_error("Invalid OBJ index");
        auto resolved = value > 0 ? static_cast<long long>(value) - 1 : static_cast<long long>(count) + value;
        if (resolved < 0 || resolved >= static_cast<long long>(count))
            throw std::runtime_error("OBJ index out of range");
        return static_cast<size_t>(resolved);
    };
    try
    {
        std::string line;
        while (std::getline(file, line))
        {
            ++lineNumber;
            std::istringstream row(line);
            std::string kind;
            row >> kind;
            if (kind == "v" || kind == "vn")
            {
                std::array<float, 3> value;
                if (!(row >> value[0] >> value[1] >> value[2]))
                    throw std::runtime_error("Invalid OBJ vector");
                for (float component : value)
                    if (!std::isfinite(component))
                        throw std::runtime_error("Non-finite OBJ vector");
                value[2] = -value[2];
                (kind == "v" ? positions : normals).push_back(value);
            }
            else if (kind == "vt")
            {
                std::array<float, 2> value;
                if (!(row >> value[0] >> value[1]) || !std::isfinite(value[0]) || !std::isfinite(value[1]))
                    throw std::runtime_error("Invalid OBJ UV");
                value[1] = 1.f - value[1];
                uvs.push_back(value);
            }
            else if (kind == "f")
            {
                std::vector<ModelVertex> face;
                std::vector<bool> hasNormal;
                std::string token;
                while (row >> token && token[0] != '#')
                {
                    auto first = token.find('/');
                    auto second = first == std::string::npos ? first : token.find('/', first + 1);
                    ModelVertex vertex;
                    vertex.position = positions.at(index(token.substr(0, first), positions.size()));
                    if (first != std::string::npos)
                    {
                        auto uv = token.substr(first + 1, second == std::string::npos ? second : second - first - 1);
                        if (!uv.empty())
                            vertex.uv = uvs.at(index(uv, uvs.size()));
                    }
                    bool present = second != std::string::npos && second + 1 < token.size();
                    if (present)
                        vertex.normal = normals.at(index(token.substr(second + 1), normals.size()));
                    face.push_back(vertex);
                    hasNormal.push_back(present);
                }
                if (face.size() < 3)
                    throw std::runtime_error("OBJ face needs at least three vertices");
                for (size_t j = 1; j + 1 < face.size(); ++j)
                {
                    // Eksen yansıması üçgenin köşe sırasını ters çevirir.
                    // Reflecting an axis reverses the triangle winding.
                    size_t corners[] = {0, j + 1, j};
                    auto a = face[corners[0]].position, b = face[corners[1]].position, c = face[corners[2]].position;
                    std::array<float, 3> ab{}, ac{}, n{};
                    for (int k = 0; k < 3; ++k)
                    {
                        ab[k] = b[k] - a[k];
                        ac[k] = c[k] - a[k];
                    }
                    n = {ab[1] * ac[2] - ab[2] * ac[1], ab[2] * ac[0] - ab[0] * ac[2], ab[0] * ac[1] - ab[1] * ac[0]};
                    float length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
                    if (length < 1e-10f)
                        continue;
                    for (float &component : n)
                        component /= length;
                    for (size_t corner : corners)
                    {
                        auto vertex = face[corner];
                        if (!hasNormal[corner])
                            vertex.normal = n;
                        else
                        {
                            auto &normal = vertex.normal;
                            float normalLength =
                                std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
                            if (normalLength < 1e-10f)
                                normal = n;
                            else
                                for (float &component : normal)
                                    component /= normalLength;
                        }
                        output.push_back(vertex);
                    }
                }
            }
        }
        if (output.empty())
            throw std::runtime_error("OBJ has no triangles");
    }
    catch (const std::exception &error)
    {
        throw std::runtime_error(path.string() + ":" + std::to_string(lineNumber) + ": " + error.what());
    }
    return output;
}
