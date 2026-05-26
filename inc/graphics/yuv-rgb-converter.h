#ifndef YUV_RGB_CONVERTER_H_
#define YUV_RGB_CONVERTER_H_

#include <d3d11_4.h>
#include <wrl/client.h>
#include <utils/non-copyable.h>

class YuvRgbConverter : NonCopyable
{
public:
    YuvRgbConverter() = default;
    ~YuvRgbConverter() = default;

    // NV12 to RGBA
    ID3D11Texture2D * convert(ID3D11Device * dev, ID3D11Texture2D * tex);

private:
    bool init_video_processor(ID3D11Device * dev, uint width, uint height);

    Microsoft::WRL::ComPtr<ID3D11VideoDevice>               _video_device;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>  _video_enum;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessor>            _video_processor;
    
    Microsoft::WRL::ComPtr<ID3D11Texture2D>                 _rgba_texture;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView>  _output_view;

    uint    _width = 0;
    uint    _height = 0;
};

#endif /* YUV_RGB_CONVERTER_H_ */