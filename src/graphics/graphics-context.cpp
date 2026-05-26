#include <common.h>
#include <graphics/graphics-context.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

bool GraphicsContext::init(GLFWwindow* window)
{
    if (!window)
    {
        LOG_ERROR("Invalid GLFW window provided to GraphicsContext!\n");
        return false;
    }

    HWND hwnd = glfwGetWin32Window(window);
    if (!create_device_d3d(hwnd))
    {
        LOG_ERROR("Failed to create D3D11 device and swap chain!\n");
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11Multithread> multithread;
    HRESULT hr = _device.As(&multithread);
    if (SUCCEEDED(hr) && multithread)
    {
        multithread->SetMultithreadProtected(TRUE);
        LOG("D3D11 Multithread protection ENABLED!\n");
    }
    else
        LOG_ERROR("Warning: Failed to enable D3D11 Multithread protection!\n");

    return true;
}

void GraphicsContext::shutdown()
{
    cleanup_render_target();
    
    _swap_chain.Reset();
    _context.Reset();
    _device.Reset();
}

void GraphicsContext::resize(uint width, uint height)
{
    if (width == 0 || height == 0) 
        return;

    if (!_swap_chain) 
        return;

    _context->OMSetRenderTargets(0, nullptr, nullptr);
    _main_rtv.Reset(); 

    HRESULT hr = _swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) 
    {
        LOG_ERROR("Failed to resize SwapChain buffers!\n");
        return;
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> tmp_buffer;
    _swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), &tmp_buffer);
    _device->CreateRenderTargetView(tmp_buffer.Get(), nullptr, &_main_rtv);

    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(width);
    vp.Height = static_cast<float>(height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;

    _context->RSSetViewports(1, &vp);
}

void GraphicsContext::present()
{
    if (_swap_chain)
        _swap_chain->Present(1, 0); // 1 = enable V-Sync
}

void GraphicsContext::clear_render_target(const Colors& colors)
{
    if (_context && _main_rtv)
    {
        ID3D11RenderTargetView* rtv = _main_rtv.Get();
        _context->OMSetRenderTargets(1, &rtv, nullptr);
        _context->ClearRenderTargetView(rtv, colors);
    }
}

bool GraphicsContext::create_device_d3d(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };

    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd,
        &_swap_chain, &_device, &featureLevel, &_context);

    if (res == DXGI_ERROR_UNSUPPORTED) // Fallback for WARP
    {
        res = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags,
            featureLevelArray, 2, D3D11_SDK_VERSION, &sd,
            &_swap_chain, &_device, &featureLevel, &_context);
    }

    if (res != S_OK)
        return false;

    create_render_target();
    return true;
}

void GraphicsContext::create_render_target()
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pBackBuffer;
    _swap_chain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    _device->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &_main_rtv);
}

void GraphicsContext::cleanup_render_target()
{
    _main_rtv.Reset();
}