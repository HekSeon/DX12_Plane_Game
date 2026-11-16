#include "Airport.h"
#include "WorldGenerator.h"
using namespace DirectX;
namespace
{
void Quad(MeshData &mesh, XMFLOAT3 a, XMFLOAT3 b, XMFLOAT3 c, XMFLOAT3 d)
{
    auto n =
        XMVector3Normalize(XMVector3Cross(XMLoadFloat3(&b) - XMLoadFloat3(&a), XMLoadFloat3(&c) - XMLoadFloat3(&a)));
    XMFLOAT3 normal;
    XMStoreFloat3(&normal, n);
    uint32_t base = uint32_t(mesh.vertices.size());
    mesh.vertices.insert(mesh.vertices.end(),
                         {{a, normal, {0, 0}}, {b, normal, {1, 0}}, {c, normal, {1, 1}}, {d, normal, {0, 1}}});
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}
XMFLOAT3 Point(float x, float z, float h)
{
    XMFLOAT3 p;
    XMStoreFloat3(&p, WorldGenerator::AirportPoint(x, z, h));
    return p;
}
void Ground(MeshData &mesh, float x0, float x1, float z0, float z1, float h)
{
    // Pisti bölerek küreye oturt; tek büyük yüzey gezegenin içine girer.
    // Subdivide the runway to follow the sphere; one large quad cuts through it.
    int count = int((z1 - z0) / 20) + 1;
    for (int i = 0; i < count; ++i)
    {
        float a = z0 + (z1 - z0) * i / count, b = z0 + (z1 - z0) * (i + 1) / count;
        Quad(mesh, Point(x0, a, h), Point(x0, b, h), Point(x1, b, h), Point(x1, a, h));
    }
}
void Building(MeshData &walls, MeshData &roofs, float x, float z, float w, float length, float height)
{
    XMFLOAT3 a = Point(x - w / 2, z - length / 2, 25), b = Point(x + w / 2, z - length / 2, 25),
             c = Point(x + w / 2, z + length / 2, 25), d = Point(x - w / 2, z + length / 2, 25);
    XMFLOAT3 A = Point(x - w / 2, z - length / 2, 25 + height), B = Point(x + w / 2, z - length / 2, 25 + height),
             C = Point(x + w / 2, z + length / 2, 25 + height), D = Point(x - w / 2, z + length / 2, 25 + height);
    Quad(walls, a, A, B, b);
    Quad(walls, b, B, C, c);
    Quad(walls, c, C, D, d);
    Quad(walls, d, D, A, a);
    Quad(roofs, A, D, C, B);
}
} // namespace
std::vector<AirportPart> Airport::Generate()
{
    std::vector<AirportPart> parts(6);
    parts[0].color = {.12f, .14f, .16f, 1};
    parts[1].color = {.96f, .95f, .83f, 1};
    parts[2].color = {.66f, .69f, .71f, 1};
    parts[3].color = {.22f, .32f, .4f, 1};
    parts[4].color = {.2f, .75f, 1, 1};
    parts[5].color = parts[3].color;
    auto &asphalt = parts[0].mesh;
    auto &paint = parts[1].mesh;
    Ground(asphalt, -32, 32, -640, 640, 26);
    Ground(asphalt, 55, 85, -570, 570, 26);
    Ground(asphalt, 32, 85, -490, -460, 26);
    Ground(asphalt, 32, 85, 460, 490, 26);
    Ground(asphalt, 85, 180, -260, 260, 26);
    for (int z = -540; z <= 540; z += 65)
        Ground(paint, -1.5f, 1.5f, float(z), float(z + 27), 26.15f);
    Ground(paint, -30, -29, -620, 620, 26.15f);
    Ground(paint, 29, 30, -620, 620, 26.15f);
    for (int k = 0; k < 6; ++k)
    {
        float x = -23.f + k * 8;
        Ground(paint, x, x + 3, -610, -565, 26.15f);
        Ground(paint, x, x + 3, 565, 610, 26.15f);
    }
    Building(parts[2].mesh, parts[3].mesh, 135, -170, 65, 85, 26);
    Building(parts[2].mesh, parts[3].mesh, 135, 0, 65, 85, 26);
    Building(parts[2].mesh, parts[3].mesh, 135, 175, 20, 20, 50);
    Building(parts[4].mesh, parts[5].mesh, 135, 175, 32, 32, 8);
    // Kontrol kabinini kulenin üstüne taşı.
    // Place the control cabin on top of the tower.
    for (int part : {4, 5})
        for (auto &vertex : parts[part].mesh.vertices)
        {
            auto p = XMLoadFloat3(&vertex.position);
            p += XMVector3Normalize(p) * 50.f;
            XMStoreFloat3(&vertex.position, p);
        }
    return parts;
}
