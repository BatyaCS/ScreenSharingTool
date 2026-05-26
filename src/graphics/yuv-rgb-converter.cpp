#include <common.h>
#include <graphics/yuv-rgb-converter.h>

ID3D11Texture2D * YuvRgbConverter::convert(ID3D11Device * dev, ID3D11Texture2D * tex)
{
    if (!dev || !tex) 
        return nullptr;

    D3D11_TEXTURE2D_DESC desc;
    tex->GetDesc(&desc);

    if (DXGI_FORMAT_NV12 != desc.Format)
    {
        LOG_ERROR("Can't convert %u format to rgba, expected NV12 format!\n");
        return nullptr;
    }

    if (_width != desc.Width || _height != desc.Height)
        if (!init_video_processor(dev, desc.Width, desc.Height)) 
            return nullptr;

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> ctx;
    dev->GetImmediateContext(&ctx);
    Microsoft::WRL::ComPtr<ID3D11VideoContext> video_ctx;
    ctx.As(&video_ctx);

    // Input View for NV12 frame
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC in_desc = {};
    in_desc.FourCC = 0;
    in_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    in_desc.Texture2D.MipSlice = 0;
    in_desc.Texture2D.ArraySlice = 0;

    Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> input_view;
    HRESULT hr = _video_device->CreateVideoProcessorInputView(tex, _video_enum.Get(), &in_desc, &input_view);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create Video Processor input view!\n");
        return nullptr;
    }

    D3D11_VIDEO_PROCESSOR_STREAM stream = {};
    stream.Enable = TRUE;
    stream.pInputSurface = input_view.Get();

    // convert NV12 to RGBA
    hr = video_ctx->VideoProcessorBlt(_video_processor.Get(), _output_view.Get(), 0, 1, &stream);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to convert NV12 to RGBA texture!\n");
        return nullptr;
    }

    return _rgba_texture.Get();
}

bool YuvRgbConverter::init_video_processor(ID3D11Device * dev, uint width, uint height)
{
    _width = width; 
    _height = height;

    HRESULT hr = dev->QueryInterface(IID_PPV_ARGS(&_video_device));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to query D3D11 device interface!\n");
        return false;
    }

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC content_desc = {};
    content_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    content_desc.InputWidth = width;
    content_desc.InputHeight = height;
    content_desc.OutputWidth = width;
    content_desc.OutputHeight = height;
    content_desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    // Doesn't matter which values to use for PROGRESSIVE FORMAT, but still need to be set for D3D11
    content_desc.InputFrameRate = { 60, 1 };
    content_desc.OutputFrameRate = { 60, 1 };

    hr = _video_device->CreateVideoProcessorEnumerator(&content_desc, &_video_enum);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create video processor enumerator!\n");
        return false;
    }

    hr = _video_device->CreateVideoProcessor(_video_enum.Get(), 0, &_video_processor);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create video processor!\n");
        return false;
    }

    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width = width;
    tex_desc.Height = height;
    tex_desc.MipLevels = 1;
    tex_desc.ArraySize = 1;
    tex_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET;

    hr = dev->CreateTexture2D(&tex_desc, nullptr, &_rgba_texture);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create D3D 2D texture!\n");
        return false;
    }

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC out_desc = {};
    out_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    
    hr = _video_device->CreateVideoProcessorOutputView(_rgba_texture.Get(), _video_enum.Get(), &out_desc, &_output_view);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create video processor output view!\n");
        return false;
    }

    return true;
}