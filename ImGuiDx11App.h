// ImGuiDx11App: bundles Win32 window + D3D11 device + ImGui context and
// drives the main loop. The UI is provided by a caller-supplied callback.

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Win32Window.h"
#include "UiWindow.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;

class ImGuiDx11App
{
public:
    using UiCallback = std::function<void()>;

    ImGuiDx11App(const std::wstring& title, int width, int height);
    ~ImGuiDx11App();

    ImGuiDx11App(const ImGuiDx11App&) = delete;
    ImGuiDx11App& operator=(const ImGuiDx11App&) = delete;

    // Register an ImGui window. The App holds only a weak reference: the
    // caller MUST keep its own shared_ptr alive for as long as the window
    // should be displayed. When the caller's shared_ptr is released, the
    // window is destroyed immediately and the App will silently drop the
    // expired entry on the next frame.
    void AddWindow(const std::shared_ptr<UiWindow>& w);

    // Run the main loop until the host window is closed.
    // Each frame draws all registered UiWindows first, then invokes the
    // optional callback for ad-hoc UI.
    void Run(const UiCallback& ui = nullptr);

    // Background clear color (RGBA). Defaults to a dark blue-gray.
    void SetClearColor(float r, float g, float b, float a);

private:
    void CreateDeviceAndSwapChain();
    void DestroyDeviceAndSwapChain();
    void CreateRenderTarget();
    void DestroyRenderTarget();

    std::unique_ptr<Win32Window>         window_;
    std::vector<std::weak_ptr<UiWindow>> ui_windows_;

    ID3D11Device*           device_  = nullptr;
    ID3D11DeviceContext*    context_ = nullptr;
    IDXGISwapChain*         swap_chain_ = nullptr;
    ID3D11RenderTargetView* rtv_     = nullptr;

    float clear_color_[4] = { 0.10f, 0.12f, 0.18f, 1.0f };
};