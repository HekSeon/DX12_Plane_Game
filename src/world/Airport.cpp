#include "Airport.h"
#include "WorldGenerator.h"
#include <algorithm>
#include <cmath>
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
const char *Airport::Name(int index)
{
    return index == 0 ? "ALPHA" : "BRAVO";
}
XMMATRIX Airport::Frame(int index, uint32_t seed)
{
    // Seed yalnızca yerleşimi belirler; arazi üretim sırası meydanları değiştirmez.
    // The seed alone determines placement; terrain generation order cannot move airports.
    struct Layout
    {
        uint32_t seed = 0;
        bool ready = false;
        XMMATRIX frames[2];
    };
    thread_local Layout layout;
    if (!layout.ready || layout.seed != seed)
    {
        uint32_t state = seed ^ 0x9e3779b9u;
        auto random = [&]() {
            state += 0x9e3779b9u;
            uint32_t value = state;
            value = (value ^ (value >> 16)) * 0x7feb352du;
            value = (value ^ (value >> 15)) * 0x846ca68bu;
            value ^= value >> 16;
            return float(value & 0xffffffu) / 16777216.f;
        };
        float u = random(), a = random() * XM_2PI, b = random() * XM_2PI;
        auto rotation = XMVectorSet(std::sqrt(1 - u) * std::sin(a), std::sqrt(1 - u) * std::cos(a),
                                    std::sqrt(u) * std::sin(b), std::sqrt(u) * std::cos(b));
        layout.frames[0] = XMMatrixRotationQuaternion(rotation);
        float distance = 3500.f + random() * 2000.f;
        float bearing = random() * XM_2PI, runway = random() * XM_2PI;
        layout.frames[1] = XMMatrixRotationY(runway) * XMMatrixRotationX(distance / WorldGenerator::Radius) *
                           XMMatrixRotationY(bearing) * layout.frames[0];
        layout.seed = seed;
        layout.ready = true;
    }
    return layout.frames[index == 0 ? 0 : 1];
}
XMVECTOR Airport::Center(int index, uint32_t seed)
{
    return XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), Frame(index, seed));
}
XMVECTOR Airport::LocalPosition(FXMVECTOR position, int index, uint32_t seed)
{
    auto local = XMVector3TransformNormal(XMVector3Normalize(position), XMMatrixTranspose(Frame(index, seed)));
    float y = XMVectorGetY(local);
    if (y < .8f)
        return XMVectorSet(1e6f, 1e6f, 1e6f, 0);
    return XMVectorSet(WorldGenerator::Radius * XMVectorGetX(local) / y,
                       XMVectorGetX(XMVector3Length(position)) - WorldGenerator::Radius,
                       WorldGenerator::Radius * XMVectorGetZ(local) / y, 0);
}
float Airport::Distance(FXMVECTOR position, int index, uint32_t seed)
{
    return std::acos(
               std::clamp(XMVectorGetX(XMVector3Dot(XMVector3Normalize(position), Center(index, seed))), -1.f, 1.f)) *
           WorldGenerator::Radius;
}
bool Airport::OnPavement(FXMVECTOR position, uint32_t seed)
{
    for (int i = 0; i < Count; ++i)
    {
        auto p = LocalPosition(position, i, seed);
        float x = XMVectorGetX(p), z = XMVectorGetZ(p);
        if ((std::abs(x) < 32 && std::abs(z) < 640) || (x > 55 && x < 85 && std::abs(z) < 570) ||
            (x > 85 && x < 180 && std::abs(z) < 260) || (x > 32 && x < 85 && std::abs(std::abs(z) - 475) < 15))
            return true;
    }
    return false;
}
int Airport::RefuelZone(FXMVECTOR position, bool grounded, float speed, float throttle, uint32_t seed)
{
    // Dolum yalnızca yerde, motor rölantide ve uçak park alanında dururken yapılır.
    // Refuel only on the ground, at idle, while stopped inside the service bay.
    if (!grounded || speed > 1.f || throttle > .02f)
        return -1;
    for (int i = 0; i < Count; ++i)
    {
        auto p = LocalPosition(position, i, seed);
        if (std::abs(XMVectorGetX(p) - 120) < 20 && std::abs(XMVectorGetZ(p) - 90) < 20 &&
            std::abs(XMVectorGetY(p) - 30) < 3)
            return i;
    }
    return -1;
}
std::vector<AirportPart> Airport::Generate(uint32_t seed)
{
    std::vector<AirportPart> parts(7);
    parts[0].color = {.12f, .14f, .16f, 1};
    parts[1].color = {.96f, .95f, .83f, 1};
    parts[2].color = {.66f, .69f, .71f, 1};
    parts[3].color = {.22f, .32f, .4f, 1};
    parts[4].color = {.2f, .75f, 1, 1};
    parts[5].color = parts[3].color;
    parts[6].color = {1.f, .7f, .08f, 1};
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
    // Sarı çerçeve ve F işareti dolum alanını pistten ayırır.
    // A yellow border and F marking distinguish the service bay from the runway.
    auto &fuel = parts[6].mesh;
    Ground(fuel, 100, 102, 70, 110, 26.2f);
    Ground(fuel, 138, 140, 70, 110, 26.2f);
    Ground(fuel, 100, 140, 70, 72, 26.2f);
    Ground(fuel, 100, 140, 108, 110, 26.2f);
    Ground(fuel, 114, 117, 81, 100, 26.2f);
    Ground(fuel, 114, 128, 97, 100, 26.2f);
    Ground(fuel, 114, 125, 89, 92, 26.2f);
    Building(parts[6].mesh, parts[3].mesh, 165, 90, 12, 18, 9);
    std::vector<AirportPart> result(parts.size());
    for (size_t part = 0; part < parts.size(); ++part)
    {
        result[part].color = parts[part].color;
        for (int airport = 0; airport < Count; ++airport)
        {
            auto matrix = Frame(airport, seed);
            auto base = uint32_t(result[part].mesh.vertices.size());
            for (auto vertex : parts[part].mesh.vertices)
            {
                XMStoreFloat3(&vertex.position, XMVector3TransformNormal(XMLoadFloat3(&vertex.position), matrix));
                XMStoreFloat3(&vertex.normal, XMVector3TransformNormal(XMLoadFloat3(&vertex.normal), matrix));
                result[part].mesh.vertices.push_back(vertex);
            }
            for (auto index : parts[part].mesh.indices)
                result[part].mesh.indices.push_back(base + index);
        }
    }
    return result;
}
