#include <common.h>
#include <graphics/cross-device-bridge.h>

ID3D11Texture2D * CrossDeviceTextureBridge::transfer(ID3D11Device * src_dev, ID3D11Texture2D * src_tex, ID3D11Device * dst_dev)
{
    if (!src_dev || !src_tex || !dst_dev)
    {
        LOG_ERROR("Wrong args provided!\n");
        return nullptr;
    }

    if (src_dev == dst_dev) 
        return src_tex;

    std::lock_guard<std::mutex> lock(_mutex);
    if (!init_textures(src_dev, src_tex, dst_dev))
        return nullptr;

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> src_ctx;
    src_dev->GetImmediateContext(&src_ctx);
    src_ctx->CopyResource(_shared_tex_src.Get(), src_tex);
    
    // TODO: double check this, probably GPU may not flush texture rightaway
    src_ctx->Flush(); 

    return _shared_tex_dst.Get();
}

bool CrossDeviceTextureBridge::init_textures(ID3D11Device * src_dev, ID3D11Texture2D * src_tex, ID3D11Device * dst_dev)
{
    D3D11_TEXTURE2D_DESC desc;
    src_tex->GetDesc(&desc);

    if (desc.Width == _width && desc.Height == _height && desc.Format == _format)
        return true;

    _shared_tex_src.Reset();
    _shared_tex_dst.Reset();
    _shared_handle = nullptr;

    D3D11_TEXTURE2D_DESC shared_desc = desc;
    shared_desc.MipLevels = 1;
    shared_desc.ArraySize = 1;
    shared_desc.SampleDesc.Count = 1;
    shared_desc.Usage = D3D11_USAGE_DEFAULT;
    shared_desc.CPUAccessFlags = 0; 
    
    // set flags to make texture shared
    shared_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE; 
    shared_desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    HRESULT hr = src_dev->CreateTexture2D(&shared_desc, nullptr, &_shared_tex_src);
    if (FAILED(hr)) 
    {
        LOG_ERROR("Failed to create D3D 2D texture to share!\n");
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIResource> dxgi_res;
    hr = _shared_tex_src.As(&dxgi_res);
    if (FAILED(hr)) 
    {
        LOG_ERROR("Failed to get system HANDLE for shared texture!\n");
        return false;
    }
    
    hr = dxgi_res->GetSharedHandle(&_shared_handle);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to get shared HANDLE for shared texture!\n");
        return false;
    }

    hr = dst_dev->OpenSharedResource(_shared_handle, __uuidof(ID3D11Texture2D), (void**)&_shared_tex_dst);
    if (FAILED(hr)) 
    {
        LOG_ERROR("Failed to open HANDLE for shared texture on dst device!\n");
        return false;
    }

    _width = desc.Width;
    _height = desc.Height;
    _format = desc.Format;
    return true;
}