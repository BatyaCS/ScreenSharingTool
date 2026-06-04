#ifndef TEXTURE_SCALER_H_
#define TEXTURE_SCALER_H_

#include <d3d11.h>
#include <d3d11_1.h>
#include <wrl/client.h>

class TextureScaler
{
public:
    TextureScaler() = default;
    ~TextureScaler() = default;

    ID3D11Texture2D * resize(ID3D11Device * dev, ID3D11Texture2D * tex, const D3D11_TEXTURE2D_DESC& out_desc);

private:
    bool init_video_processor(ID3D11Device * dev, DXGI_FORMAT format, uint in_width, uint in_height, uint out_width, uint out_height);

    Microsoft::WRL::ComPtr<ID3D11VideoDevice>               _video_device;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>  _video_enum;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessor>            _video_processor;
    
    Microsoft::WRL::ComPtr<ID3D11Texture2D>                 _internal_input_texture;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView>   _input_view;

    Microsoft::WRL::ComPtr<ID3D11Texture2D>                 _resized_texture;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView>  _output_view;

    DXGI_FORMAT _format = DXGI_FORMAT_UNKNOWN;
    uint        _in_width = 0;
    uint        _in_height = 0;
    uint        _out_width = 0;
    uint        _out_height = 0;
};

#endif /* TEXTURE_SCALER_H_ */