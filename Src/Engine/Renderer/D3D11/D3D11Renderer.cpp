#include "D3D11Renderer.h"
#include "Engine/Renderer/Renderer.h"

#include "Core/Config.h"
#include "Core/Utility.h"

#include <d3d11.h>
#include <dxgi1_6.h>

struct Renderer
{
    IDXGIFactory2* Factory;
    IDXGIAdapter1* Adapter;
    ID3D11Device* Device;
    ID3D11DeviceContext* Context;
    IDXGISwapChain1* SwapChain;
    ID3D11RenderTargetView* RenderTargetView;
    uint32 BackBufferWidth, BackBufferHeight;
    bool TearingSupported;
    uint32 Flags;
};
static Renderer RendererData;

bool D3D11RendererInit(HWND windowHandle)
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
