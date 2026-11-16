#include "dx12renderer.h"
#include "shadersource.h"
#include "game/game.h"
#include "hud/hud.h"
#include "hud/hudshader.h"
#include "world/sea.h"
#include <map>
#include <d3d12.h>
#include "model/model.h"
#include <cfloat>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <wincodec.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <vector>
#ifdef HAVE_ASSIMP
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#endif
using Microsoft::WRL::ComPtr;
using namespace DirectX;
namespace
{
constexpr float Radius = WorldGenerator::Radius;
struct Mesh
{
    ComPtr<ID3D12Resource> vertices, indices;
    D3D12_VERTEX_BUFFER_VIEW vb{};
    D3D12_INDEX_BUFFER_VIEW ib{};
    UINT count{};
};
void Check(HRESULT hr)
{
    if (FAILED(hr))
    {
        std::ostringstream s;
        s << "DirectX operation failed: 0x" << std::hex << unsigned(hr);
        throw std::runtime_error(s.str());
    }
}
D3D12_HEAP_PROPERTIES Heap(D3D12_HEAP_TYPE type)
{
    D3D12_HEAP_PROPERTIES h{};
    h.Type = type;
    return h;
}
D3D12_RESOURCE_DESC Buffer(UINT64 bytes)
{
    D3D12_RESOURCE_DESC d{};
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width = bytes;
    d.Height = 1;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.SampleDesc.Count = 1;
    d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return d;
}
D3D12_RESOURCE_BARRIER Transition(ID3D12Resource *r, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
{
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition = {r, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, from, to};
    return b;
}

} // namespace

struct DX12Renderer::Impl
{
    HWND window;
    UINT width, height, frame = 0, rtvStep = 0;
    UINT64 fenceValue = 0;
    HANDLE eventHandle = nullptr;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<IDXGISwapChain3> swap;
    ComPtr<ID3D12DescriptorHeap> rtvs, dsvs, srvs;
    std::array<ComPtr<ID3D12Resource>, 2> targets;
    ComPtr<ID3D12Resource> depth;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> pipeline, skyPipeline, hudPipeline;
    std::array<ComPtr<ID3D12Resource>, 12> textures;
    Hud hud;
    HudAtlas hudAtlas;
    ComPtr<ID3D12Resource> hudBuffer;
    HudVertex *hudMapped = nullptr;
    static constexpr size_t HudCapacity = 32768;
    UINT nightWidth = 640, nightHeight = 480;
    std::vector<ComPtr<ID3D12Resource>> staging;
    const Game &game;
    Mesh sea, plane;
    std::map<ChunkId, Mesh> chunks;
    std::vector<Mesh> airport;
    bool comOwned = false;
    explicit Impl(HWND w, UINT x, UINT y, const Game &scene) : window(w), width(x), height(y), game(scene)
    {
        HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        comOwned = SUCCEEDED(com);
        Initialize();
    }
    ~Impl()
    {
        if (queue && fence && eventHandle)
        {
            try
            {
                Wait();
            }
            catch (...)
            {
            }
        }
        if (hudBuffer && hudMapped)
            hudBuffer->Unmap(0, nullptr);
        if (eventHandle)
            CloseHandle(eventHandle);
        if (comOwned)
            CoUninitialize();
    }
    void Wait()
    {
        Check(queue->Signal(fence.Get(), ++fenceValue));
        if (fence->GetCompletedValue() < fenceValue)
        {
            Check(fence->SetEventOnCompletion(fenceValue, eventHandle));
            if (WaitForSingleObject(eventHandle, INFINITE) != WAIT_OBJECT_0)
                throw std::runtime_error("GPU wait failed");
        }
    }
    ComPtr<ID3D12Resource> Upload(const void *data, UINT64 size)
    {
        ComPtr<ID3D12Resource> r;
        auto h = Heap(D3D12_HEAP_TYPE_UPLOAD);
        auto d = Buffer(size);
        Check(device->CreateCommittedResource(&h, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                              IID_PPV_ARGS(&r)));
        void *mapped;
        D3D12_RANGE range{0, 0};
        Check(r->Map(0, &range, &mapped));
        std::memcpy(mapped, data, size);
        r->Unmap(0, nullptr);
        return r;
    }
    Mesh MakeMesh(const std::vector<Vertex> &v, const std::vector<UINT> &i)
    {
        Mesh m;
        m.vertices = Upload(v.data(), v.size() * sizeof(Vertex));
        m.indices = Upload(i.data(), i.size() * sizeof(UINT));
        m.vb = {m.vertices->GetGPUVirtualAddress(), UINT(v.size() * sizeof(Vertex)), sizeof(Vertex)};
        m.ib = {m.indices->GetGPUVirtualAddress(), UINT(i.size() * sizeof(UINT)), DXGI_FORMAT_R32_UINT};
        m.count = UINT(i.size());
        return m;
    }
    void Texture(UINT slot, const wchar_t *filename, std::array<unsigned char, 4> fallback,
                 const HudAtlas *atlas = nullptr)
    {
        UINT tw = 1, th = 1;
        std::vector<unsigned char> pixels(fallback.begin(), fallback.end());
        if (filename && std::filesystem::exists(filename))
        {
            ComPtr<IWICImagingFactory> factory;
            Check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
            ComPtr<IWICBitmapDecoder> decoder;
            Check(factory->CreateDecoderFromFilename(filename, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
                                                     &decoder));
            ComPtr<IWICBitmapFrameDecode> bitmap;
            Check(decoder->GetFrame(0, &bitmap));
            Check(bitmap->GetSize(&tw, &th));
            if (tw > 8192 || th > 8192 || !tw || !th)
                throw std::runtime_error("Texture dimensions must be between 1 and 8192");
            ComPtr<IWICFormatConverter> converter;
            Check(factory->CreateFormatConverter(&converter));
            Check(converter->Initialize(bitmap.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0,
                                        WICBitmapPaletteTypeCustom));
            pixels.resize(size_t(tw) * th * 4);
            Check(converter->CopyPixels(nullptr, tw * 4, UINT(pixels.size()), pixels.data()));
        }
        if (atlas)
        {
            tw = HudAtlas::Width;
            th = HudAtlas::Height;
            pixels = atlas->pixels;
        }
        if (slot == 8)
        {
            nightWidth = tw;
            nightHeight = th;
        }
        D3D12_RESOURCE_DESC d{};
        d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        d.Width = tw;
        d.Height = th;
        d.DepthOrArraySize = 1;
        d.MipLevels = 1;
        d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        d.SampleDesc.Count = 1;
        auto heap = Heap(D3D12_HEAP_TYPE_DEFAULT);
        Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                              IID_PPV_ARGS(&textures[slot])));
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT64 bytes;
        device->GetCopyableFootprints(&d, 0, 1, 0, &footprint, nullptr, nullptr, &bytes);
        std::vector<unsigned char> padded(size_t(bytes), 0);
        for (UINT row = 0; row < th; ++row)
            std::memcpy(padded.data() + footprint.Offset + size_t(row) * footprint.Footprint.RowPitch,
                        pixels.data() + size_t(row) * tw * 4, tw * 4);
        auto upload = Upload(padded.data(), bytes);
        D3D12_TEXTURE_COPY_LOCATION dst{};
        dst.pResource = textures[slot].Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION src{};
        src.pResource = upload.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = footprint;
        list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        auto barrier = Transition(textures[slot].Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        list->ResourceBarrier(1, &barrier);
        staging.push_back(upload);
        D3D12_SHADER_RESOURCE_VIEW_DESC view{};
        view.Format = d.Format;
        view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        view.Texture2D.MipLevels = 1;
        auto handle = srvs->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += size_t(slot) * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        device->CreateShaderResourceView(textures[slot].Get(), &view, handle);
    }
    Mesh Sea()
    {
        constexpr UINT rows = 256, cols = 512;
        std::vector<Vertex> vertices;
        std::vector<UINT> indices;
        for (UINT y = 0; y <= rows; ++y)
            for (UINT x = 0; x <= cols; ++x)
            {
                float lat = XM_PI * y / rows, lon = XM_2PI * x / cols;
                auto n = XMVectorSet(std::sin(lat) * std::cos(lon), std::cos(lat), std::sin(lat) * std::sin(lon), 0);
                Vertex vertex{};
                XMStoreFloat3(&vertex.position, n * Radius);
                XMStoreFloat3(&vertex.normal, n);
                vertex.uv = {float(x) / cols, float(y) / rows};
                vertices.push_back(vertex);
            }
        for (UINT y = 0; y < rows; ++y)
            for (UINT x = 0; x < cols; ++x)
            {
                UINT a = y * (cols + 1) + x, b = a + cols + 1;
                indices.insert(indices.end(), {a, b, a + 1, a + 1, b, b + 1});
            }
        return MakeMesh(vertices, indices);
    }
    void SyncChunks()
    {
        // Önceki kare GPU'da tamamlandı; artık kullanılmayan tamponlar bırakılabilir.
        // The previous GPU frame has completed, so unused buffers can be released.
        const auto &source = game.Chunks();
        for (auto it = chunks.begin(); it != chunks.end();)
        {
            if (!source.contains(it->first))
                it = chunks.erase(it);
            else
                ++it;
        }
        for (const auto &[id, data] : source)
            if (!chunks.contains(id))
                chunks.emplace(id, MakeMesh(data.vertices, data.indices));
    }
    Mesh Plane()
    {
        // OBJ dışa aktarımı hazırsa ek bir model kütüphanesine gerek yok.
        // Use the OBJ export when available; no external importer is needed.
        const char *objPath =
            std::filesystem::exists("data/plane.obj") ? "data/plane.obj" : "data/model/Airplane/airplane.obj";
        if (std::filesystem::exists(objPath))
        {
            auto source = Model::Load(objPath);
            std::vector<Vertex> vertices;
            std::vector<UINT> indices;
            std::array<float, 3> lo{FLT_MAX, FLT_MAX, FLT_MAX}, hi{-FLT_MAX, -FLT_MAX, -FLT_MAX};
            for (auto &v : source)
                for (int axis = 0; axis < 3; ++axis)
                {
                    lo[axis] = std::min(lo[axis], v.position[axis]);
                    hi[axis] = std::max(hi[axis], v.position[axis]);
                }
            float scale = 24.f / std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2], .001f});
            bool bundled = std::string(objPath) == "data/model/Airplane/airplane.obj";
            for (auto &v : source)
            {
                Vertex vertex{{(v.position[0] - (lo[0] + hi[0]) * .5f) * scale,
                               (v.position[1] - (lo[1] + hi[1]) * .5f) * scale,
                               (v.position[2] - (lo[2] + hi[2]) * .5f) * scale},
                              {v.normal[0], v.normal[1], v.normal[2]},
                              {v.uv[0], v.uv[1]}};
                // Eksen dönüşümünden sonra bu modelin burnunu +Z yönüne çevir.
                // Restore this model's +Z nose after the handedness conversion.
                if (bundled)
                {
                    vertex.position.x = -vertex.position.x;
                    vertex.position.z = -vertex.position.z;
                    vertex.normal.x = -vertex.normal.x;
                    vertex.normal.z = -vertex.normal.z;
                }
                indices.push_back(UINT(vertices.size()));
                vertices.push_back(vertex);
            }
            return MakeMesh(vertices, indices);
        }
#ifdef HAVE_ASSIMP
        const char *fbxPath =
            std::filesystem::exists("data/plane.fbx") ? "data/plane.fbx" : "data/model/Airplane/airplane.fbx";
        if (std::filesystem::exists(fbxPath))
        {
            Assimp::Importer importer;
            const aiScene *scene =
                importer.ReadFile(fbxPath, aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                                               aiProcess_PreTransformVertices | aiProcess_ConvertToLeftHanded);
            if (!scene || !scene->HasMeshes())
                throw std::runtime_error(importer.GetErrorString());
            std::vector<Vertex> vertices;
            std::vector<UINT> indices;
            for (UINT m = 0; m < scene->mNumMeshes; ++m)
            {
                auto mesh = scene->mMeshes[m];
                UINT base = UINT(vertices.size());
                for (UINT j = 0; j < mesh->mNumVertices; ++j)
                {
                    auto p = mesh->mVertices[j];
                    auto n = mesh->mNormals[j];
                    auto uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][j] : aiVector3D{};
                    vertices.push_back({{p.x, p.y, p.z}, {n.x, n.y, n.z}, {uv.x, uv.y}});
                }
                for (UINT j = 0; j < mesh->mNumFaces; ++j)
                {
                    auto &face = mesh->mFaces[j];
                    if (face.mNumIndices == 3)
                        for (UINT k = 0; k < 3; ++k)
                            indices.push_back(base + face.mIndices[k]);
                }
            }
            if (vertices.empty() || indices.empty())
                throw std::runtime_error("plane.fbx has no triangles");
            XMFLOAT3 lo = vertices[0].position, hi = lo;
            for (auto &v : vertices)
            {
                lo.x = std::min(lo.x, v.position.x);
                lo.y = std::min(lo.y, v.position.y);
                lo.z = std::min(lo.z, v.position.z);
                hi.x = std::max(hi.x, v.position.x);
                hi.y = std::max(hi.y, v.position.y);
                hi.z = std::max(hi.z, v.position.z);
            }
            float scale = 24.f / std::max({hi.x - lo.x, hi.y - lo.y, hi.z - lo.z, .001f});
            for (auto &v : vertices)
            {
                v.position.x = (v.position.x - (hi.x + lo.x) * .5f) * scale;
                v.position.y = (v.position.y - (hi.y + lo.y) * .5f) * scale;
                v.position.z = (v.position.z - (hi.z + lo.z) * .5f) * scale;
            }
            return MakeMesh(vertices, indices);
        }
#endif
        std::vector<Vertex> v;
        std::vector<UINT> i;
        auto tri = [&](XMFLOAT3 a, XMFLOAT3 b, XMFLOAT3 c) {
            auto n = XMVector3Normalize(
                XMVector3Cross(XMLoadFloat3(&b) - XMLoadFloat3(&a), XMLoadFloat3(&c) - XMLoadFloat3(&a)));
            XMFLOAT3 normal;
            XMStoreFloat3(&normal, n);
            UINT base = UINT(v.size());
            v.insert(v.end(), {{a, normal, {0, 0}}, {b, normal, {1, 0}}, {c, normal, {.5f, 1}}});
            i.insert(i.end(), {base, base + 1, base + 2});
        };
        XMFLOAT3 nose{0, 0, 13}, tail{0, 0, -10}, left{-1.5f, 0, 0}, right{1.5f, 0, 0}, top{0, 2, 0}, bottom{0, -1, 0};
        tri(nose, left, top);
        tri(nose, top, right);
        tri(tail, top, left);
        tri(tail, right, top);
        tri(nose, bottom, left);
        tri(nose, right, bottom);
        tri(tail, left, bottom);
        tri(tail, bottom, right);
        tri({-15, 0, -3}, {15, 0, -3}, {0, .4f, 4});
        tri({-6, .5f, -9}, {6, .5f, -9}, {0, .5f, -5});
        tri({0, 0, -10}, {0, 5, -9}, {0, 0, -4});
        return MakeMesh(v, i);
    }
    void Targets()
    {
        auto h = rtvs->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < 2; ++i)
        {
            Check(swap->GetBuffer(i, IID_PPV_ARGS(&targets[i])));
            device->CreateRenderTargetView(targets[i].Get(), nullptr, h);
            h.ptr += rtvStep;
        }
        D3D12_RESOURCE_DESC d{};
        d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        d.Width = width;
        d.Height = height;
        d.DepthOrArraySize = 1;
        d.MipLevels = 1;
        d.Format = DXGI_FORMAT_D32_FLOAT;
        d.SampleDesc.Count = 1;
        d.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        D3D12_CLEAR_VALUE clear{};
        clear.Format = d.Format;
        clear.DepthStencil.Depth = 1;
        auto heap = Heap(D3D12_HEAP_TYPE_DEFAULT);
        Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear,
                                              IID_PPV_ARGS(&depth)));
        device->CreateDepthStencilView(depth.Get(), nullptr, dsvs->GetCPUDescriptorHandleForHeapStart());
    }
    void Initialize()
    {
        UINT flags = 0;
#ifdef _DEBUG
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
        {
            debug->EnableDebugLayer();
            flags = DXGI_CREATE_FACTORY_DEBUG;
        }
#endif
        ComPtr<IDXGIFactory6> factory;
        Check(CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory)));
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT n = 0; factory->EnumAdapterByGpuPreference(n, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                             IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
             ++n)
        {
            DXGI_ADAPTER_DESC1 desc;
            Check(adapter->GetDesc1(&desc));
            if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) &&
                SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
                break;
            adapter.Reset();
        }
        if (!device)
        {
            Check(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)));
            Check(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
        }
        D3D12_COMMAND_QUEUE_DESC q{};
        Check(device->CreateCommandQueue(&q, IID_PPV_ARGS(&queue)));
        DXGI_SWAP_CHAIN_DESC1 sd{};
        sd.Width = width;
        sd.Height = height;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 2;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        ComPtr<IDXGISwapChain1> initial;
        Check(factory->CreateSwapChainForHwnd(queue.Get(), window, &sd, nullptr, nullptr, &initial));
        Check(initial.As(&swap));
        Check(factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER));
        frame = swap->GetCurrentBackBufferIndex();
        D3D12_DESCRIPTOR_HEAP_DESC hd{};
        hd.NumDescriptors = 2;
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        Check(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtvs)));
        rtvStep = device->GetDescriptorHandleIncrementSize(hd.Type);
        hd.NumDescriptors = 1;
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        Check(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&dsvs)));
        hd.NumDescriptors = 12;
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        Check(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&srvs)));
        Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
        Check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
                                        IID_PPV_ARGS(&list)));
        Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!eventHandle)
            throw std::runtime_error("Could not create GPU fence event");
        Targets();
        D3D12_DESCRIPTOR_RANGE range{};
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.NumDescriptors = 2;
        D3D12_ROOT_PARAMETER parameters[2]{};
        parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[0].Constants = {0, 0, 60};
        parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[1].DescriptorTable = {1, &range};
        parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC rs{};
        rs.NumParameters = 2;
        rs.pParameters = parameters;
        rs.NumStaticSamplers = 1;
        rs.pStaticSamplers = &sampler;
        rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        ComPtr<ID3DBlob> blob, error;
        Check(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error));
        Check(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root)));
        auto compile = [&](const char *entry, const char *target, const char *source = SceneShader) {
            ComPtr<ID3DBlob> code, errors;
            // Deniz ayarlarının tek kaynağı sea.h; shader sabitlerini buradan üret.
            // Keep sea.h as the source of truth for the shader's wave constants.
            const std::string waveValues[] = {std::to_string(Sea::PrimaryAmplitude),
                                              std::to_string(Sea::SecondaryAmplitude),
                                              std::to_string(Sea::PrimaryFrequency),
                                              std::to_string(Sea::SecondaryFrequency),
                                              std::to_string(Sea::Speed),
                                              std::to_string(Sea::SecondarySpeed)};
            const D3D_SHADER_MACRO waveDefines[] = {{"SEA_PRIMARY_AMPLITUDE", waveValues[0].c_str()},
                                                    {"SEA_SECONDARY_AMPLITUDE", waveValues[1].c_str()},
                                                    {"SEA_PRIMARY_FREQUENCY", waveValues[2].c_str()},
                                                    {"SEA_SECONDARY_FREQUENCY", waveValues[3].c_str()},
                                                    {"SEA_PRIMARY_SPEED", waveValues[4].c_str()},
                                                    {"SEA_SECONDARY_SPEED", waveValues[5].c_str()},
                                                    {nullptr, nullptr}};
            HRESULT hr = D3DCompile(source, std::strlen(source), nullptr, waveDefines, nullptr, entry, target,
                                    D3DCOMPILE_ENABLE_STRICTNESS, 0, &code, &errors);
            if (FAILED(hr))
                throw std::runtime_error(errors ? static_cast<const char *>(errors->GetBufferPointer())
                                                : "Shader compilation failed");
            return code;
        };
        auto vs = compile("VS", "vs_5_0"), ps = compile("PS", "ps_5_0");
        D3D12_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
        D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
        p.pRootSignature = root.Get();
        p.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
        p.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
        p.InputLayout = {layout, 3};
        p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        p.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        p.RasterizerState.DepthClipEnable = TRUE;
        p.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        p.SampleMask = UINT_MAX;
        p.DepthStencilState.DepthEnable = TRUE;
        p.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        p.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        p.NumRenderTargets = 1;
        p.RTVFormats[0] = sd.Format;
        p.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        p.SampleDesc.Count = 1;
        Check(device->CreateGraphicsPipelineState(&p, IID_PPV_ARGS(&pipeline)));
        // Gökyüzü derinlik yazmadan çizilir; sahne geometrisi onun üzerine gelir.
        // Draw the sky without writing depth; scene geometry is rendered over it.
        auto skyVS = compile("SkyVS", "vs_5_0"), skyPS = compile("SkyPS", "ps_5_0");
        p.VS = {skyVS->GetBufferPointer(), skyVS->GetBufferSize()};
        p.PS = {skyPS->GetBufferPointer(), skyPS->GetBufferSize()};
        p.InputLayout = {nullptr, 0};
        p.DepthStencilState.DepthEnable = FALSE;
        p.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        Check(device->CreateGraphicsPipelineState(&p, IID_PPV_ARGS(&skyPipeline)));
        auto hudVS = compile("HudVS", "vs_5_0", HudShader), hudPS = compile("HudPS", "ps_5_0", HudShader);
        D3D12_INPUT_ELEMENT_DESC hudLayout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
        p.InputLayout = {hudLayout, 3};
        p.VS = {hudVS->GetBufferPointer(), hudVS->GetBufferSize()};
        p.PS = {hudPS->GetBufferPointer(), hudPS->GetBufferSize()};
        auto &blend = p.BlendState.RenderTarget[0];
        blend.BlendEnable = TRUE;
        blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blend.BlendOp = D3D12_BLEND_OP_ADD;
        blend.SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        Check(device->CreateGraphicsPipelineState(&p, IID_PPV_ARGS(&hudPipeline)));
        hudAtlas = Hud::CreateFontAtlas();
        auto uploadHeap = Heap(D3D12_HEAP_TYPE_UPLOAD);
        auto hudDesc = Buffer(HudCapacity * sizeof(HudVertex));
        Check(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &hudDesc,
                                              D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&hudBuffer)));
        D3D12_RANGE noRead{0, 0};
        Check(hudBuffer->Map(0, &noRead, reinterpret_cast<void **>(&hudMapped)));

        Texture(0, std::filesystem::exists("data/field.png") ? L"data/field.png" : L"data/texture/field005.jpg",
                {92, 133, 63, 255});
        Texture(1, L"data/texture/field004.jpg", {110, 105, 88, 255});
        Texture(2, std::filesystem::exists("data/sea.png") ? L"data/sea.png" : L"data/texture/sea.jpg",
                {24, 109, 170, 255});
        Texture(3, nullptr, {128, 128, 255, 255});
        Texture(4, L"data/texture/plane_diffuse.png", {236, 183, 61, 255});
        Texture(5, L"data/texture/plane_normal.png", {128, 128, 255, 255});
        Texture(6, nullptr, {255, 255, 255, 255});
        Texture(7, nullptr, {128, 128, 255, 255});
        Texture(8, L"data/texture/night.jpg", {2, 3, 8, 255});
        Texture(9, nullptr, {128, 128, 255, 255});
        Texture(10, nullptr, {255, 255, 255, 255}, &hudAtlas);
        Texture(11, nullptr, {255, 255, 255, 255});
        sea = Sea();
        plane = Plane();
        SyncChunks();
        for (const auto &part : game.AirportParts())
            airport.push_back(MakeMesh(part.mesh.vertices, part.mesh.indices));
        Check(list->Close());
        ID3D12CommandList *commands[] = {list.Get()};
        queue->ExecuteCommandLists(1, commands);
        Wait();
        staging.clear();
    }
    void Draw()
    {
        SyncChunks();
        const auto &player = game.GetPlayer();
        auto position = player.Position(), forward = player.Forward(), camera = player.Camera();
        Check(allocator->Reset());
        Check(list->Reset(allocator.Get(), pipeline.Get()));
        auto barrier =
            Transition(targets[frame].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        list->ResourceBarrier(1, &barrier);
        auto rtv = rtvs->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += size_t(frame) * rtvStep;
        auto dsv = dsvs->GetCPUDescriptorHandleForHeapStart();
        const auto &fog = game.GetFog();
        const float sky[] = {fog.color.x, fog.color.y, fog.color.z, 1};
        list->ClearRenderTargetView(rtv, sky, 0, nullptr);
        list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1, 0, 0, nullptr);
        list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        D3D12_VIEWPORT viewport{0, 0, float(width), float(height), 0, 1};
        D3D12_RECT scissor{0, 0, LONG(width), LONG(height)};
        list->RSSetViewports(1, &viewport);
        list->RSSetScissorRects(1, &scissor);
        list->SetGraphicsRootSignature(root.Get());
        ID3D12DescriptorHeap *heaps[] = {srvs.Get()};
        list->SetDescriptorHeaps(1, heaps);
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        auto up = XMVector3Normalize(position);
        auto view = XMMatrixLookAtLH(camera, position + forward * 25, up);
        auto projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, float(width) / height, 1.f, 35000.f);
        struct Constants
        {
            XMFLOAT4X4 mvp, model;
            XMFLOAT4 tint, eye, options, fogColor, fogRange, sunDirection, sunColor;
        };
        static_assert(sizeof(Constants) == 60 * sizeof(float));
        const auto lighting = game.Lighting();
        Constants skyConstants{};
        // Gökyüzü düzlemlerini gezegenin yerel yukarı yönüne hizala.
        // Align the sky planes with the planet's local up direction.
        auto reference = std::abs(XMVectorGetZ(up)) > .98f ? XMVectorSet(1, 0, 0, 0) : XMVectorSet(0, 0, 1, 0);
        auto skyRight = XMVector3Normalize(XMVector3Cross(up, reference));
        auto skyForward = XMVector3Normalize(XMVector3Cross(skyRight, up));
        XMMATRIX skyFrame(skyRight, up, skyForward, XMVectorSet(0, 0, 0, 1));
        XMStoreFloat4x4(&skyConstants.model, skyFrame);
        XMStoreFloat4x4(&skyConstants.mvp, view * projection);
        // Çift karo sayısı karşılıklı düzlem kenarlarında aynı ayna sınırını kullanır.
        // Even tile counts place matching mirror boundaries on opposite plane edges.
        float tiles = std::max(6.f, std::ceil(float(width) / float(nightWidth) / 6.f) * 6.f);
        skyConstants.options = {tiles, tiles * float(nightWidth) / float(nightHeight), 0, 0};
        XMStoreFloat4(&skyConstants.eye, camera);
        skyConstants.fogColor = fog.color;
        skyConstants.sunDirection = lighting.sunDirection;
        skyConstants.sunColor = lighting.sunColor;
        list->SetPipelineState(skyPipeline.Get());
        list->SetGraphicsRoot32BitConstants(0, 60, &skyConstants, 0);
        auto nightSrv = srvs->GetGPUDescriptorHandleForHeapStart();
        nightSrv.ptr += UINT64(8) * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        list->SetGraphicsRootDescriptorTable(1, nightSrv);
        list->DrawInstanced(36, 1, 0, 0);
        list->SetPipelineState(pipeline.Get());
        auto draw = [&](const Mesh &mesh, FXMMATRIX model, UINT texture, float repeat, float material, XMFLOAT4 tint) {
            Constants constants{};
            XMStoreFloat4x4(&constants.mvp, model * view * projection);
            XMStoreFloat4x4(&constants.model, model);
            constants.tint = tint;
            XMStoreFloat4(&constants.eye, camera);
            constants.options = {repeat, material, game.SeaTime(), 0};
            constants.fogColor = fog.color;
            constants.fogRange = {fog.start, fog.end, fog.enabled ? 1.f : 0.f, Radius};
            constants.sunDirection = lighting.sunDirection;
            constants.sunColor = lighting.sunColor;
            list->SetGraphicsRoot32BitConstants(0, 60, &constants, 0);
            auto srv = srvs->GetGPUDescriptorHandleForHeapStart();
            srv.ptr +=
                UINT64(texture) * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            list->SetGraphicsRootDescriptorTable(1, srv);
            list->IASetVertexBuffers(0, 1, &mesh.vb);
            list->IASetIndexBuffer(&mesh.ib);
            list->DrawIndexedInstanced(mesh.count, 1, 0, 0, 0);
        };
        draw(sea, XMMatrixIdentity(), 2, 32, 1, {1, 1, 1, 1});
        for (const auto &[id, chunk] : chunks)
            draw(chunk, XMMatrixIdentity(), 0, 1, 0, {1, 1, 1, 1});
        for (size_t i = 0; i < airport.size(); ++i)
            draw(airport[i], XMMatrixIdentity(), 6, 1, 3, game.AirportParts()[i].color);
        draw(plane, player.WorldMatrix(), 4, 1, 2, {1, 1, 1, 1});
        // HUD en son ve derinlik testi olmadan çizilir; dünya tarafından örtülmez.
        // Draw the HUD last without depth testing so the world cannot occlude it.
        hud.Build(game, width, height, hudAtlas);
        const auto &overlay = hud.Vertices();
        if (overlay.size() > HudCapacity)
            throw std::runtime_error("HUD vertex capacity exceeded");
        std::memcpy(hudMapped, overlay.data(), overlay.size() * sizeof(HudVertex));
        D3D12_VERTEX_BUFFER_VIEW hudView{hudBuffer->GetGPUVirtualAddress(), UINT(overlay.size() * sizeof(HudVertex)),
                                         sizeof(HudVertex)};
        list->SetPipelineState(hudPipeline.Get());
        const float hudConstants[] = {float(width), float(height), 0, 0};
        list->SetGraphicsRoot32BitConstants(0, 4, hudConstants, 0);
        auto atlasHandle = srvs->GetGPUDescriptorHandleForHeapStart();
        atlasHandle.ptr +=
            UINT64(10) * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        list->SetGraphicsRootDescriptorTable(1, atlasHandle);
        list->IASetVertexBuffers(0, 1, &hudView);
        list->DrawInstanced(UINT(overlay.size()), 1, 0, 0);

        barrier = Transition(targets[frame].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        list->ResourceBarrier(1, &barrier);
        Check(list->Close());
        ID3D12CommandList *commands[] = {list.Get()};
        queue->ExecuteCommandLists(1, commands);
        Check(swap->Present(1, 0));
        Wait();
        frame = swap->GetCurrentBackBufferIndex();
    }
    void Resize(UINT x, UINT y)
    {
        if (!x || !y || (x == width && y == height))
            return;
        Wait();
        for (auto &target : targets)
            target.Reset();
        depth.Reset();
        width = x;
        height = y;
        Check(swap->ResizeBuffers(2, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0));
        frame = swap->GetCurrentBackBufferIndex();
        Targets();
    }
};
DX12Renderer::DX12Renderer() = default;
DX12Renderer::~DX12Renderer() = default;

bool DX12Renderer::Initialize(HWND window, uint32_t width, uint32_t height, const Game &game)
{
    m_impl = std::make_unique<Impl>(window, width, height, game);
    return true;
}
void DX12Renderer::Shutdown()
{
    m_impl.reset();
}
void DX12Renderer::Render()
{
    if (m_impl)
        m_impl->Draw();
}
void DX12Renderer::Resize(uint32_t width, uint32_t height)
{
    if (m_impl)
        m_impl->Resize(width, height);
}
