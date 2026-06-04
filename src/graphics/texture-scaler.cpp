#include <common.h>
#include <graphics/texture-scaler.h>

// TODO: probably need to be combined into one class for resizing/converting
ID3D11Texture2D * TextureScaler::resize(ID3D11Device * dev, ID3D11Texture2D * tex, const D3D11_TEXTURE2D_DESC& out_desc)
{
    if (!dev || !tex || out_desc.Width == 0 || out_desc.Height == 0) 
        return nullptr;

    D3D11_TEXTURE2D_DESC desc;
    tex->GetDesc(&desc);

    if (desc.Width == out_desc.Width && desc.Height == out_desc.Height)
        return tex;

    if (_in_width != desc.Width || _in_height != desc.Height || 
        _out_width != out_desc.Width || _out_height != out_desc.Height || _format != desc.Format)
    {
        if (!init_video_processor(dev, desc.Format, desc.Width, desc.Height, out_desc.Width, out_desc.Height)) 
            return nullptr;
    }

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> ctx;
    dev->GetImmediateContext(&ctx);
    Microsoft::WRL::ComPtr<ID3D11VideoContext> video_ctx;
    ctx.As(&video_ctx);
    ctx->CopyResource(_internal_input_texture.Get(), tex);

    D3D11_VIDEO_PROCESSOR_STREAM stream = {};
    stream.Enable = TRUE;
    stream.pInputSurface = _input_view.Get();
    
    // FullRGB -> FullRGB
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE color_space = {};
    color_space.RGB_Range = 0;
    color_space.YCbCr_Matrix = 0;
    color_space.YCbCr_xvYCC = 0;
    color_space.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255;

    video_ctx->VideoProcessorSetStreamColorSpace(_video_processor.Get(), 0, &color_space);
    video_ctx->VideoProcessorSetOutputColorSpace(_video_processor.Get(), &color_space);

    const HRESULT hr = video_ctx->VideoProcessorBlt(_video_processor.Get(), _output_view.Get(), 0, 1, &stream);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to resize texture via VideoProcessorBlt!\n");
        return nullptr;
    }

    return _resized_texture.Get();
}

bool TextureScaler::init_video_processor(ID3D11Device * dev, DXGI_FORMAT format, uint in_width, uint in_height, uint out_width, uint out_height)
{
    HRESULT hr = dev->QueryInterface(IID_PPV_ARGS(&_video_device));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to query D3D11 video device interface!\n");
        return false;
    }

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC content_desc = {};
    content_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    content_desc.InputWidth = in_width;
    content_desc.InputHeight = in_height;
    content_desc.OutputWidth = out_width;
    content_desc.OutputHeight = out_height;
    content_desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    // Values below doesn't matter
    content_desc.InputFrameRate = { 60, 1 };
    content_desc.OutputFrameRate = { 60, 1 };

    hr = _video_device->CreateVideoProcessorEnumerator(&content_desc, &_video_enum);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create video processor enumerator for scaler!\n");
        return false;
    }

    hr = _video_device->CreateVideoProcessor(_video_enum.Get(), 0, &_video_processor);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create video processor for scaler!\n");
        return false;
    }

    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width = out_width;
    tex_desc.Height = out_height;
    tex_desc.MipLevels = 1;
    tex_desc.ArraySize = 1;
    tex_desc.Format = format;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET;

    hr = dev->CreateTexture2D(&tex_desc, nullptr, &_resized_texture);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create target D3D 2D texture for scaler!\n");
        return false;
    }

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC out_desc = {};
    out_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    out_desc.Texture2D.MipSlice = 0;
    
    hr = _video_device->CreateVideoProcessorOutputView(_resized_texture.Get(), _video_enum.Get(), &out_desc, &_output_view);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create video processor output view for scaler!\n");
        return false;
    }

    D3D11_TEXTURE2D_DESC in_tex_desc = {};
    in_tex_desc.Width = in_width;
    in_tex_desc.Height = in_height;
    in_tex_desc.MipLevels = 1;
    in_tex_desc.ArraySize = 1;
    in_tex_desc.Format = format; 
    in_tex_desc.SampleDesc.Count = 1;
    in_tex_desc.Usage = D3D11_USAGE_DEFAULT;
    in_tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    hr = dev->CreateTexture2D(&in_tex_desc, nullptr, &_internal_input_texture);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create internal input texture for scaler!\n");
        return false;
    }

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC in_view_desc = {};
    in_view_desc.FourCC = 0;
    in_view_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    in_view_desc.Texture2D.MipSlice = 0;
    in_view_desc.Texture2D.ArraySlice = 0;

    hr = _video_device->CreateVideoProcessorInputView(_internal_input_texture.Get(), _video_enum.Get(), &in_view_desc, &_input_view);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create video processor input view for scaler!\n");
        return false;
    }

    _in_width = in_width; 
    _in_height = in_height;
    _out_width = out_width;
    _out_height = out_height;
    _format = format;

    return true;
}