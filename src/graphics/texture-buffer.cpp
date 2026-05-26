#include <common.h>
#include <graphics/texture-buffer.h>

bool TextureBuffer::resize(ID3D11Device * device, uint width, uint height)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_width == width && _height == height && _texture)
        return true;

    _srv.Reset();
    _texture.Reset();

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; 
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT; 
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = device->CreateTexture2D(&desc, nullptr, &_texture);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create D3D11 Texture2D!\n");
        return false;
    }

    hr = device->CreateShaderResourceView(_texture.Get(), nullptr, &_srv);
    if (FAILED(hr)) 
    {
        LOG_ERROR("Failed to create D3D11 Shader Resource View!\n");
        return false;
    }

    _width = width;
    _height = height;
    return true;
}

void TextureBuffer::copy_from(ID3D11DeviceContext* ctx, ID3D11Texture2D* source_tex)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    if (!_texture || !source_tex) 
        return;

    // D3D11Multithread protection should be enabled
    ctx->CopySubresourceRegion(_texture.Get(), 0, 0, 0, 0, source_tex, 0, nullptr);
}