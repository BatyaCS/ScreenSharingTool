#ifndef CROSS_DEVICE_BRIDGE_H_
#define CROSS_DEVICE_BRIDGE_H_

#include <d3d11_4.h>
#include <wrl/client.h>
#include <mutex>
#include <utils/non-copyable.h>

class CrossDeviceTextureBridge : NonCopyable
{
public:
    CrossDeviceTextureBridge() = default;

    ID3D11Texture2D * transfer(ID3D11Device * src_dev, ID3D11Texture2D * src_tex, ID3D11Device * dst_dev);

private:
    bool init_textures(ID3D11Device * src_dev, ID3D11Texture2D * src_tex, ID3D11Device * dst_dev);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> _shared_tex_src;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> _shared_tex_dst;
    
    HANDLE                                  _shared_handle = nullptr;
    DXGI_FORMAT                             _format = DXGI_FORMAT_UNKNOWN;

    std::mutex  _mutex;

    uint        _width = 0;
    uint        _height = 0;
};

#endif /* CROSS_DEVICE_BRIDGE_H_ */