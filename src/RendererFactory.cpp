#include "RendererFactory.h"
#include "DX12/dx12renderer.h"

std::unique_ptr<IRenderer> CreateRenderer()
{
    return std::make_unique<DX12Renderer>();
}
