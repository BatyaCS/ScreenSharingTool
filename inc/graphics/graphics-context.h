#ifndef GRAPHICS_CONTEXT_H_
#define GRAPHICS_CONTEXT_H_

#include <d3d11_4.h>
#include <wrl/client.h>

#include <utils/non-copyable.h>

struct GLFWwindow; 

class GraphicsContext : NonCopyable
{
public:
    using Colors = float[4];

    GraphicsContext() = default;
    ~GraphicsContext() { shutdown(); }

    bool init(GLFWwindow* window);
    void shutdown();

    bool is_visible() const;
    void resize(uint width, uint height);
    
    void present();
    void clear_render_target(const Colors& colors);

    ID3D11Device * get_device() const { return _device.Get(); }
    ID3D11DeviceContext * get_context() const { return _context.Get(); }
    ID3D11RenderTargetView * get_rtv() const { return _main_rtv.Get(); }

private:
    bool create_device_d3d(HWND hwnd);
    void create_render_target();
    void cleanup_render_target();

    Microsoft::WRL::ComPtr<ID3D11Device>           _device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>    _context;
    Microsoft::WRL::ComPtr<IDXGISwapChain>         _swap_chain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> _main_rtv;
};

#endif /* GRAPHICS_CONTEXT_H_ */