#ifndef TEXTURE_BUFFER_H_
#define TEXTURE_BUFFER_H_

#include <d3d11_4.h>
#include <wrl/client.h>
#include <mutex>

// TODO: For now just works for BGRA texture, nothing fancy
class TextureBuffer
{
public:
    TextureBuffer() = default;
    ~TextureBuffer() = default;

    bool resize(ID3D11Device * device, uint width, uint height);
    void copy_from(ID3D11DeviceContext* ctx, ID3D11Texture2D* source_tex);

    void * get_texture() { std::lock_guard<std::mutex> lock(_mutex); return reinterpret_cast<void*>(_srv.Get()); }

    // TODO: theoretically getters are not safe
    uint width()  { std::lock_guard<std::mutex> lock(_mutex); return _width; }
    uint height() { std::lock_guard<std::mutex> lock(_mutex); return _height; }

private:
    std::mutex _mutex;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> _texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _srv;
    
    uint _width = 0;
    uint _height = 0;
};

#endif /* TEXTURE_BUFFER_H_ */