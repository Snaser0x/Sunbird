#include "D3D11Renderer.h"
#include "Engine/Renderer/Renderer.h"

#include "Core/Config.h"
#include "Core/Utility.h"

#include <d3d11.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

// NOTE(saeb): 16384 quads * 4 = 65536 vertices, the most a 16-bit index can address.
#define SB_MAX_QUADS 16384
static_assert(SB_MAX_QUADS * 4 <= 65536, "Sunbird: Quad vertices must be addressable by 16-bit indices.");

#define SB_MAX_PIPELINES 64
#define SB_MAX_TEXTURES 1024

struct QuadVertex
{
    real32 X, Y; // Position
    real32 U, V; // Texture Coordinates
    real32 R, G, B, A; // Color
};

struct QuadBatch
{
    RendererPipeline Pipeline;
    RendererTexture Texture;
    uint32 FirstQuad;
    uint32 QuadCount;
};

struct QuadPipeline
{
    ID3D11PixelShader* PixelShader;
};

struct QuadConstants
{
    real32 ScreenWidth, ScreenHeight;
    real32 Padding[2]; // Constant buffers are sized in 16-byte multiples
};

struct Renderer
{
    IDXGIFactory2* Factory;
    IDXGIAdapter1* Adapter;
    ID3D11Device* Device;
    ID3D11DeviceContext* Context;
    IDXGISwapChain1* SwapChain;
    uint32 BackBufferWidth, BackBufferHeight;
    bool TearingSupported;
    ID3D11RenderTargetView* RenderTargetView;
    ID3D11Buffer* VertexBuffer;
    ID3D11Buffer* IndexBuffer;
    RendererQuad* Quads;
    uint32 QuadCount;
    QuadBatch* Batches;
    ID3D11VertexShader* QuadVertexShader;
    ID3D11InputLayout* QuadInputLayout;
    ID3D11Buffer* QuadConstantBuffer;
    ID3D11BlendState* BlendState;
    ID3D11RasterizerState* RasterizerState;
    ID3D11SamplerState* SamplerState;
    QuadPipeline Pipelines[SB_MAX_PIPELINES];
    uint32 PipelineCount;
    ID3D11ShaderResourceView* Textures[SB_MAX_TEXTURES];
    uint32 TextureCount;
    uint32 Flags;
    
};
static Renderer RendererData;

static const char QuadShaderSource[] = R"(
cbuffer QuadConstants : register(b0)
{
    float2 ScreenSize;
    float2 Padding;
};

Texture2D QuadTexture : register(t0);
SamplerState QuadSampler : register(s0);

struct VSInput
{
    float2 Position : POSITION;
    float2 UV : TEXCOORD;
    float4 Color : COLOR;
};

struct PSInput
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD;
    float4 Color : COLOR;
};

PSInput VSMain(VSInput input)
{
    PSInput output;

    // Pixels (top-left origin, y down) -> clip space (center origin, y up).
    float2 clip = (input.Position / ScreenSize) * float2(2.0, -2.0) + float2(-1.0, 1.0);
    output.Position = float4(clip, 0.0, 1.0);
    output.UV = input.UV;
    output.Color = input.Color;

    return output;
}

float4 PSMain(PSInput input) : SV_Target
{
    return QuadTexture.Sample(QuadSampler, input.UV) * input.Color;
}
)";

static ID3DBlob* D3D11CompileShader(const char* source, usize sourceSize, const char* entryPoint, const char* target)
{
    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION; // Readable in RenderDoc / PIX
#else
    compileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    ID3DBlob* bytecode = nullptr;
    ID3DBlob* errors = nullptr;
    HRESULT result = D3DCompile(source, sourceSize, nullptr, nullptr, nullptr, entryPoint, target, compileFlags, 0, &bytecode, &errors);

    // NOTE(saeb): Errors and warnings both come back here, with line numbers.
    if(errors)
    {
        OutputDebugStringA((const char*)errors->GetBufferPointer());
        errors->Release();
    }

    if(FAILED(result))
    {
        return(nullptr);
    }

    return(bytecode);
}

static bool D3D11CreateTexture(uint32 width, uint32 height, const void* pixels, ID3D11ShaderResourceView** view)
{
    // NOTE(saeb): Validate first; with the debug layer set to break on errors, a bad description would stop the program instead of just failing.
    if(!pixels || width == 0 || height == 0 || width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION || height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
    {
        return(false);
    }

    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_IMMUTABLE; // Contents given at creation, never written again
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA textureData = {};
    textureData.pSysMem = pixels;
    textureData.SysMemPitch = width * 4; // Bytes per row

    ID3D11Texture2D* texture = nullptr;
    if(FAILED(RendererData.Device->CreateTexture2D(&textureDesc, &textureData, &texture)))
    {
        return(false);
    }

    HRESULT viewResult = RendererData.Device->CreateShaderResourceView(texture, nullptr, view);
    texture->Release(); // The view holds its own reference to the texture

    return(SUCCEEDED(viewResult));
}

static void D3D11FlushQuads()
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    if(FAILED(RendererData.Context->Map(RendererData.VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return;
    }

    QuadVertex* vertices = (QuadVertex*)mapped.pData;
    QuadBatch* batch = nullptr;
    uint32 batchCount = 0;

    for(uint32 quadIndex = 0; quadIndex < RendererData.QuadCount; ++quadIndex)
    {
        const RendererQuad* quad = &RendererData.Quads[quadIndex];

        // NOTE(saeb): Only consecutive quads merge; submission order is the layering order for alpha.
        if(!batch || batch->Pipeline != quad->Pipeline || batch->Texture != quad->Texture)
        {
            batch = &RendererData.Batches[batchCount++];
            batch->Pipeline = quad->Pipeline;
            batch->Texture = quad->Texture;
            batch->FirstQuad = quadIndex;
            batch->QuadCount = 0;
        }

        ++batch->QuadCount;

        real32 x0 = quad->X;
        real32 y0 = quad->Y;
        real32 x1 = quad->X + quad->Width;
        real32 y1 = quad->Y + quad->Height;

        // NOTE(saeb): The game passes straight colors; premultiply here so the blend state's ONE is correct.
        real32 r = quad->R * quad->A;
        real32 g = quad->G * quad->A;
        real32 b = quad->B * quad->A;

        // NOTE(saeb): Mapped memory is write-combined; write each vertex whole, front to back, never read it back.
        QuadVertex* quadVertices = vertices + (quadIndex * 4);
        quadVertices[0] = { x0, y0, quad->U0, quad->V0, r, g, b, quad->A }; // Top-left
        quadVertices[1] = { x1, y0, quad->U1, quad->V0, r, g, b, quad->A }; // Top-right
        quadVertices[2] = { x0, y1, quad->U0, quad->V1, r, g, b, quad->A }; // Bottom-left
        quadVertices[3] = { x1, y1, quad->U1, quad->V1, r, g, b, quad->A }; // Bottom-right
    }

    RendererData.Context->Unmap(RendererData.VertexBuffer, 0);

    UINT stride = sizeof(QuadVertex);
    UINT offset = 0;
    RendererData.Context->IASetVertexBuffers(0, 1, &RendererData.VertexBuffer, &stride, &offset);
    RendererData.Context->IASetIndexBuffer(RendererData.IndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    RendererData.Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    QuadConstants constants = {};
    constants.ScreenWidth = (real32)RendererData.BackBufferWidth;
    constants.ScreenHeight = (real32)RendererData.BackBufferHeight;
    RendererData.Context->UpdateSubresource(RendererData.QuadConstantBuffer, 0, nullptr, &constants, 0, 0);

    RendererData.Context->IASetInputLayout(RendererData.QuadInputLayout);
    RendererData.Context->VSSetShader(RendererData.QuadVertexShader, nullptr, 0);
    RendererData.Context->VSSetConstantBuffers(0, 1, &RendererData.QuadConstantBuffer);
    RendererData.Context->RSSetState(RendererData.RasterizerState);
    RendererData.Context->PSSetSamplers(0, 1, &RendererData.SamplerState);
    RendererData.Context->OMSetBlendState(RendererData.BlendState, nullptr, 0xFFFFFFFF);

    RendererPipeline boundPipeline = UINT32_MAX; // Nothing bound yet
    RendererTexture boundTexture = UINT32_MAX;

    for(uint32 batchIndex = 0; batchIndex < batchCount; ++batchIndex)
    {
        QuadBatch* current = &RendererData.Batches[batchIndex];

        // NOTE(saeb): An invalid handle falls back to the defaults instead of binding garbage.
        RendererPipeline pipeline = (current->Pipeline < RendererData.PipelineCount) ? current->Pipeline : 0;
        RendererTexture texture = (current->Texture < RendererData.TextureCount) ? current->Texture : 0;

        if(pipeline != boundPipeline)
        {
            RendererData.Context->PSSetShader(RendererData.Pipelines[pipeline].PixelShader, nullptr, 0);
            boundPipeline = pipeline;
        }

        if(texture != boundTexture)
        {
            RendererData.Context->PSSetShaderResources(0, 1, &RendererData.Textures[texture]);
            boundTexture = texture;
        }

        RendererData.Context->DrawIndexed(current->QuadCount * 6, current->FirstQuad * 6, 0);
    }
}

bool D3D11RendererInit(StackAllocator* allocator, HWND windowHandle)
{
    // NOTE(saeb): Factory first, so we choose the adapter; the swap chain later comes from this same factory, which is the adapter's parent (a mismatched factory fails).
    UINT factoryFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
    factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    if(FAILED(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&RendererData.Factory))))
    {
        // NOTE(saeb): The debug factory needs the optional "Graphics Tools" Windows feature; fall back to a normal one.
        if(FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&RendererData.Factory))))
        {
            return(false);
        }
    }

    // NOTE(saeb): IDXGIFactory6 (minimum Windows 10, version 1803) can order adapters so the high-performance GPU comes first; older systems fall back to plain enumeration order.
    IDXGIFactory6* factory6 = nullptr;
    RendererData.Factory->QueryInterface(IID_PPV_ARGS(&factory6));

    for(UINT adapterIndex = 0; ; ++adapterIndex)
    {
        HRESULT result;
        if(factory6)
        {
            result = factory6->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&RendererData.Adapter));
        }
        else
        {
            result = RendererData.Factory->EnumAdapters1(adapterIndex, &RendererData.Adapter);
        }

        if(FAILED(result))
        {
            RendererData.Adapter = nullptr;
            break;
        }

        // NOTE(saeb): Skip the Microsoft Basic Render Driver (CPU rasterizer); it's always enumerated but never what we want.
        DXGI_ADAPTER_DESC1 adapterDesc;
        RendererData.Adapter->GetDesc1(&adapterDesc);

        if(!(adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
        {
            OutputDebugStringW(L"[Sunbird] GPU: ");
            OutputDebugStringW(adapterDesc.Description);
            OutputDebugStringW(L"\n");
            break;
        }

        RendererData.Adapter->Release();
        RendererData.Adapter = nullptr;
    }

    if(factory6)
    {
        factory6->Release();
    }

    if(!RendererData.Adapter)
    {
        return(false);
    }

    UINT deviceFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
    deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };

    // NOTE(saeb): Driver type must be UNKNOWN when passing an explicit adapter; HARDWARE is only for a null adapter.
    HRESULT result = D3D11CreateDevice(RendererData.Adapter,
                                       D3D_DRIVER_TYPE_UNKNOWN,
                                       nullptr,
                                       deviceFlags,
                                       featureLevels,
                                       SB_ARRAYCOUNT(featureLevels),
                                       D3D11_SDK_VERSION,
                                       &RendererData.Device,
                                       nullptr,
                                       &RendererData.Context);
#if defined(DEBUG) || defined(_DEBUG)
    // NOTE(saeb): The debug layer ships with the optional "Graphics Tools" Windows feature; run without it rather than fail.
    if(result == DXGI_ERROR_SDK_COMPONENT_MISSING)
    {
        OutputDebugStringW(L"[Sunbird] D3D11 debug layer not installed (Settings > Optional features > Graphics Tools).\n");
        deviceFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
        result = D3D11CreateDevice(RendererData.Adapter,
                                   D3D_DRIVER_TYPE_UNKNOWN,
                                   nullptr,
                                   deviceFlags,
                                   featureLevels,
                                   SB_ARRAYCOUNT(featureLevels),
                                   D3D11_SDK_VERSION,
                                   &RendererData.Device,
                                   nullptr,
                                   &RendererData.Context);
    }
#endif

    if(FAILED(result))
    {
        return(false);
    }

#if defined(DEBUG) || defined(_DEBUG)
    // NOTE(saeb): Stop in the debugger on the exact API call that misuses D3D, instead of finding out from a black screen.
    ID3D11InfoQueue* infoQueue = nullptr;
    if(SUCCEEDED(RendererData.Device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
    {
        infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
        infoQueue->Release();
    }
#endif

    D3D11_BUFFER_DESC vertexBufferDesc = {};
    vertexBufferDesc.ByteWidth = SB_MAX_QUADS * 4 * sizeof(QuadVertex);
    vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC; // CPU writes it, GPU reads it
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if(FAILED(RendererData.Device->CreateBuffer(&vertexBufferDesc, nullptr, &RendererData.VertexBuffer)))
    {
        return(false);
    }

    Frame frameScratch = GetFrame(allocator, Heap::Upper);

    uint32 indexCount = SB_MAX_QUADS * 6;
    uint16* indices = (uint16*)Allocate(allocator, Heap::Upper, indexCount * sizeof(uint16), alignof(uint16));
    if(!indices)
    {
        ReleaseFrame(allocator, frameScratch);
        return(false);
    }

    // NOTE(saeb): Vertex order per quad: 0 = top-left, 1 = top-right, 2 = bottom-left, 3 = bottom-right.
    for(uint32 quadIndex = 0; quadIndex < SB_MAX_QUADS; ++quadIndex)
    {
        uint16 firstVertex = (uint16)(quadIndex * 4);
        uint16* quadIndices = indices + (quadIndex * 6);

        quadIndices[0] = (uint16)(firstVertex + 0);
        quadIndices[1] = (uint16)(firstVertex + 1);
        quadIndices[2] = (uint16)(firstVertex + 2);
        quadIndices[3] = (uint16)(firstVertex + 2);
        quadIndices[4] = (uint16)(firstVertex + 1);
        quadIndices[5] = (uint16)(firstVertex + 3);
    }

    D3D11_BUFFER_DESC indexBufferDesc = {};
    indexBufferDesc.ByteWidth = indexCount * sizeof(uint16);
    indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE; // Filled once at creation, never written again
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = indices;

    HRESULT indexResult = RendererData.Device->CreateBuffer(&indexBufferDesc, &indexData, &RendererData.IndexBuffer);

    // NOTE(saeb): The data is copied during CreateBuffer, so the scratch can go right away.
    ReleaseFrame(allocator, frameScratch);

    if(FAILED(indexResult))
    {
        return(false);
    }

    // NOTE(saeb): SB_MAX_QUADS (16384) * 56 bytes = 896 KiB for the quads and 16384 * 16 bytes = 256 KiB for the batches. Sizing the batch array for the worst case (every quad changes state) means no check for running out of batches.
    RendererData.Quads = (RendererQuad*)Allocate(allocator, Heap::Lower, SB_MAX_QUADS * sizeof(RendererQuad), alignof(RendererQuad));
    RendererData.Batches = (QuadBatch*)Allocate(allocator, Heap::Lower, SB_MAX_QUADS * sizeof(QuadBatch), alignof(QuadBatch));
    if(!RendererData.Quads || !RendererData.Batches)
    {
        return(false);
    }

    ID3DBlob* vertexShaderBytecode = D3D11CompileShader(QuadShaderSource, sizeof(QuadShaderSource) - 1, "VSMain", "vs_5_0");
    ID3DBlob* pixelShaderBytecode = D3D11CompileShader(QuadShaderSource, sizeof(QuadShaderSource) - 1, "PSMain", "ps_5_0");

    // NOTE(saeb): The input layout is validated against the vertex shader's input signature, so it needs the VS bytecode.
    D3D11_INPUT_ELEMENT_DESC inputElements[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(QuadVertex, X), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(QuadVertex, U), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(QuadVertex, R), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

    bool shadersCreated = vertexShaderBytecode && pixelShaderBytecode &&
        SUCCEEDED(RendererData.Device->CreateVertexShader(vertexShaderBytecode->GetBufferPointer(), vertexShaderBytecode->GetBufferSize(), nullptr, &RendererData.QuadVertexShader)) &&
        SUCCEEDED(RendererData.Device->CreatePixelShader(pixelShaderBytecode->GetBufferPointer(), pixelShaderBytecode->GetBufferSize(), nullptr, &RendererData.Pipelines[0].PixelShader)) &&
        SUCCEEDED(RendererData.Device->CreateInputLayout(inputElements, SB_ARRAYCOUNT(inputElements), vertexShaderBytecode->GetBufferPointer(), vertexShaderBytecode->GetBufferSize(), &RendererData.QuadInputLayout));

    // NOTE(saeb): The bytecode is only needed for creation.
    if(vertexShaderBytecode)
    {
        vertexShaderBytecode->Release();
    }

    if(pixelShaderBytecode)
    {
        pixelShaderBytecode->Release();
    }

    if(!shadersCreated)
    {
        return(false);
    }

    RendererData.PipelineCount = 1;

    D3D11_BUFFER_DESC constantBufferDesc = {};
    constantBufferDesc.ByteWidth = sizeof(QuadConstants);
    constantBufferDesc.Usage = D3D11_USAGE_DEFAULT; // Updated with UpdateSubresource
    constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    if(FAILED(RendererData.Device->CreateBuffer(&constantBufferDesc, nullptr, &RendererData.QuadConstantBuffer)))
    {
        return(false);
    }

    // NOTE(saeb): Premultiplied "over": color = src + dst * (1 - srcAlpha); src.rgb already carries its alpha.
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    if(FAILED(RendererData.Device->CreateBlendState(&blendDesc, &RendererData.BlendState)))
    {
        return(false);
    }

    D3D11_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_NONE; // A negative width/height flips winding; still draw it
    rasterizerDesc.DepthClipEnable = TRUE; // D3D11'S default is TRUE, but a zeroed desc makes it FALSE

    if(FAILED(RendererData.Device->CreateRasterizerState(&rasterizerDesc, &RendererData.RasterizerState)))
    {
        return(false);
    }

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX; // Zeroed would lock every texture to mip 0

    if(FAILED(RendererData.Device->CreateSamplerState(&samplerDesc, &RendererData.SamplerState)))
    {
        return(false);
    }

    // NOTE(saeb): Handle 0 is the built-in white texture; plain rects sample it, and textures that fail to create fall back to it.
    uint32 whitePixel = 0xFFFFFFFF;
    if(!D3D11CreateTexture(1, 1, (const uint8*)&whitePixel, &RendererData.Textures[0]))
    {
        return(false);
    }
    RendererData.TextureCount = 1;

    // NOTE(saeb): Tearing is what lets VSync-off actually present immediately on flip-model swap chains (needs Windows 10 + driver support).
    IDXGIFactory5* factory5 = nullptr;
    if(SUCCEEDED(RendererData.Factory->QueryInterface(IID_PPV_ARGS(&factory5))))
    {
        BOOL allowTearing = FALSE;

        if(SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
        {
            RendererData.TearingSupported = (allowTearing == TRUE);
        }

        factory5->Release();
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = 0; // 0 = take the window's client size
    swapChainDesc.Height = 0;
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1; // Flip model can't be multisampled; MSAA would be a separate target resolved into this one
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2; // Flip model minimum
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapChainDesc.Flags = RendererData.TearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    if(FAILED(RendererData.Factory->CreateSwapChainForHwnd(RendererData.Device, windowHandle, &swapChainDesc, nullptr, nullptr, &RendererData.SwapChain)))
    {
        return(false);
    }

    // NOTE(saeb): Fullscreen is the window layer's job (WindowFlags_Fullscreen); stop DXGI from hijacking Alt+Enter.
    RendererData.Factory->MakeWindowAssociation(windowHandle, DXGI_MWA_NO_ALT_ENTER);

    RendererData.SwapChain->GetDesc1(&swapChainDesc);
    RendererData.BackBufferWidth = swapChainDesc.Width;
    RendererData.BackBufferHeight = swapChainDesc.Height;

    ID3D11Texture2D* backBuffer = nullptr;
    if(FAILED(RendererData.SwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
    {
        return(false);
    }

    if(FAILED(RendererData.Device->CreateRenderTargetView(backBuffer, nullptr, &RendererData.RenderTargetView)))
    {
        backBuffer->Release();
        return(false);
    }

    backBuffer->Release();

    return(true);
}

void D3D11RendererBeginFrame(uint32 width, uint32 height)
{
    if((width != RendererData.BackBufferWidth || height != RendererData.BackBufferHeight) && width > 0 && height > 0)
    {
        // NOTE(saeb): ResizeBuffers fails while anything still references the old back buffer, so unbind and release the view first.
        RendererData.Context->OMSetRenderTargets(0, nullptr, nullptr);
        RendererData.RenderTargetView->Release();
        RendererData.RenderTargetView = nullptr;

        RendererData.SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, RendererData.TearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);

        ID3D11Texture2D* backBuffer = nullptr;
        if(FAILED(RendererData.SwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
        {
            return;
        }

        if(FAILED(RendererData.Device->CreateRenderTargetView(backBuffer, nullptr, &RendererData.RenderTargetView)))
        {
            backBuffer->Release();
            return;
        }

        backBuffer->Release();

        RendererData.BackBufferWidth = width;
        RendererData.BackBufferHeight = height;
    }

    // NOTE(saeb): A failed resize leaves no view to draw into; skip the frame until device-loss handling exists.
    if(!RendererData.RenderTargetView)
    {
        return;
    }

    // NOTE(saeb): Flip model unbinds the render target on every Present, so bind it every frame.
    RendererData.Context->OMSetRenderTargets(1, &RendererData.RenderTargetView, nullptr);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = (real32)RendererData.BackBufferWidth;
    viewport.Height = (real32)RendererData.BackBufferHeight;
    viewport.MaxDepth = 1.0f;

    RendererData.Context->RSSetViewports(1, &viewport);

    real32 clearColor[4] = { 1.0f, 0.5f, 0.0f, 1.0f };
    RendererData.Context->ClearRenderTargetView(RendererData.RenderTargetView, clearColor);
}

void D3D11RendererEndFrame()
{
    if(RendererData.RenderTargetView && RendererData.QuadCount > 0)
    {
        D3D11FlushQuads();
    }

    // NOTE(saeb): Reset here, not in BeginFrame; BeginFrame can early-out and would leave stale quads behind.
    RendererData.QuadCount = 0;

    if(RendererData.Flags & RendererFlags_VSync)
    {
        RendererData.SwapChain->Present(1, 0);
    }
    else
    {
        RendererData.SwapChain->Present(0, RendererData.TearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0);
    }   
}

void D3D11RendererShutdown()
{
    if(RendererData.Context)
    {
        RendererData.Context->ClearState();
        RendererData.Context->Flush();
    }

    if(RendererData.RenderTargetView)
    {
        RendererData.RenderTargetView->Release();
        RendererData.RenderTargetView = nullptr;
    }

    // NOTE(saeb): Walk the full arrays, not just up to the counts; a failed Init can create an object before its count is set.
    for(uint32 textureIndex = 0; textureIndex < SB_MAX_TEXTURES; ++textureIndex)
    {
        if(RendererData.Textures[textureIndex])
        {
            RendererData.Textures[textureIndex]->Release();
            RendererData.Textures[textureIndex] = nullptr;
        }
    }
    RendererData.TextureCount = 0;

    for(uint32 pipelineIndex = 0; pipelineIndex < SB_MAX_PIPELINES; ++pipelineIndex)
    {
        if(RendererData.Pipelines[pipelineIndex].PixelShader)
        {
            RendererData.Pipelines[pipelineIndex].PixelShader->Release();
            RendererData.Pipelines[pipelineIndex].PixelShader = nullptr;
        }
    }
    RendererData.PipelineCount = 0;

    if(RendererData.SamplerState)
    {
        RendererData.SamplerState->Release();
        RendererData.SamplerState = nullptr;
    }

    if(RendererData.RasterizerState)
    {
        RendererData.RasterizerState->Release();
        RendererData.RasterizerState = nullptr;
    }

    if(RendererData.BlendState)
    {
        RendererData.BlendState->Release();
        RendererData.BlendState = nullptr;
    }

    if(RendererData.QuadConstantBuffer)
    {
        RendererData.QuadConstantBuffer->Release();
        RendererData.QuadConstantBuffer = nullptr;
    }

    if(RendererData.QuadInputLayout)
    {
        RendererData.QuadInputLayout->Release();
        RendererData.QuadInputLayout = nullptr;
    }

    if(RendererData.QuadVertexShader)
    {
        RendererData.QuadVertexShader->Release();
        RendererData.QuadVertexShader = nullptr;
    }

    if(RendererData.IndexBuffer)
    {
        RendererData.IndexBuffer->Release();
        RendererData.IndexBuffer = nullptr;
    }

    if(RendererData.VertexBuffer)
    {
        RendererData.VertexBuffer->Release();
        RendererData.VertexBuffer = nullptr;
    }

    // NOTE(saeb): The quad and batch arrays live in the engine's Lower heap; ShutdownStackAllocator frees them.
    RendererData.Quads = nullptr;
    RendererData.Batches = nullptr;
    RendererData.QuadCount = 0;

    if(RendererData.SwapChain)
    {
        RendererData.SwapChain->Release();
        RendererData.SwapChain = nullptr;
    }

    if(RendererData.Context)
    {
        RendererData.Context->Release();
        RendererData.Context = nullptr;
    }

#if defined(DEBUG) || defined(_DEBUG)
    // NOTE(saeb): Anything listed here besides the device itself is a leaked COM reference.
    ID3D11Debug* debug = nullptr;
    if(RendererData.Device && SUCCEEDED(RendererData.Device->QueryInterface(IID_PPV_ARGS(&debug))))
    {
        debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
        debug->Release();
    }
#endif

    if(RendererData.Device)
    {
        RendererData.Device->Release();
        RendererData.Device = nullptr;
    }

    if(RendererData.Adapter)
    {
        RendererData.Adapter->Release();
        RendererData.Adapter = nullptr;
    }

    if(RendererData.Factory)
    {
        RendererData.Factory->Release();
        RendererData.Factory = nullptr;
    }
}

void RendererSetFlags(uint32 rendererFlags)
{
    RendererData.Flags = rendererFlags;
}

void RendererPushQuad(const RendererQuad* quad)
{
    // NOTE(saeb): Full; drop the quad rather than overflow. The vertex buffer can't hold more anyway.
    if(RendererData.QuadCount >= SB_MAX_QUADS)
    {
        return;
    }

    RendererData.Quads[RendererData.QuadCount++] = *quad;
}

RendererTexture RendererCreateTexture(uint32 width, uint32 height, const uint8* pixels)
{
    // NOTE(saeb): Not initialized, table full, or creation failed: return the white texture, so the quad still draws (white) instead of crashing.
    if(!RendererData.Device || RendererData.TextureCount >= SB_MAX_TEXTURES)
    {
        return(0);
    }

    if(!D3D11CreateTexture(width, height, pixels, &RendererData.Textures[RendererData.TextureCount]))
    {
        return(0);
    }

    return(RendererData.TextureCount++);
}
