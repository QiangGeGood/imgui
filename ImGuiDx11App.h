// ImGuiDx11App: bundles Win32 window + D3D11 device + ImGui context and
// drives the main loop. Designed so it can:
//   * Block in Run() like a standalone app, OR
//   * Run on its own UI thread (Start/Stop) so callers with their own
//     message loop can embed it for live data visualization with minimal
//     intrusion to their existing code.

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "UiWindow.h"

class Win32Window;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;

class ImGuiDx11App
{
public:
    using UiCallback = std::function<void()>;
    using Task       = std::function<void()>;

    // Stores config only; window/device are created on the UI thread.
    ImGuiDx11App(const std::wstring& title, int width, int height);
    ~ImGuiDx11App(); // auto-Stop() + join

    ImGuiDx11App(const ImGuiDx11App&) = delete;
    ImGuiDx11App& operator=(const ImGuiDx11App&) = delete;

    // --- Background mode (preferred for embedding in an existing loop) ---

    // Spawn the dedicated UI thread. Non-blocking. Safe to call once.
    void Start();

    // Signal the UI thread to exit (PostMessage WM_CLOSE) and join it.
    // Safe to call multiple times; called automatically by the destructor.
    void Stop();

    bool IsRunning() const { return running_.load(); }

    // Post a task to be executed on the UI thread at the start of the next
    // frame. Use this for one-shot initialization that must touch ImGui.
    void Post(Task task);

    // --- Blocking mode (kept for compatibility with the standalone demo) ---

    // Internally = Start() + join(). The optional callback runs every frame
    // on the UI thread (after registered UiWindows are drawn).
    void Run(const UiCallback& ui = nullptr);

    // --- Window registration (thread-safe; callable any time) ---

    // App holds only a weak_ptr; caller must keep its own shared_ptr alive.
    void AddWindow(const std::shared_ptr<UiWindow>& w);

    // --- Misc ---

    void SetClearColor(float r, float g, float b, float a);

private:
    void UiThreadMain();
    void CreateDeviceAndSwapChain();
    void DestroyDeviceAndSwapChain();
    void CreateRenderTarget();
    void DestroyRenderTarget();
    void DrawFrame(const UiCallback& per_frame_ui);

    // Config (set on owning thread, read on UI thread before Start joins).
    std::wstring title_;
    int          width_  = 0;
    int          height_ = 0;

    // Owned/used exclusively on the UI thread.
    std::unique_ptr<Win32Window> window_;
    ID3D11Device*                device_     = nullptr;
    ID3D11DeviceContext*         context_    = nullptr;
    IDXGISwapChain*              swap_chain_ = nullptr;
    ID3D11RenderTargetView*      rtv_        = nullptr;
    float                        clear_color_[4] = { 0.10f, 0.12f, 0.18f, 1.0f };

    // Thread plumbing.
    std::thread          ui_thread_;
    std::atomic<bool>    running_{ false };
    // Published by the UI thread once the window is created, used by Stop()
    // to PostMessage(WM_CLOSE) from any thread without racing on window_.
    std::atomic<void*>   hwnd_for_stop_{ nullptr };

    // Shared state (mutex-protected; touched from any thread).
    mutable std::mutex                    windows_mu_;
    std::vector<std::weak_ptr<UiWindow>>  ui_windows_;

    mutable std::mutex                    tasks_mu_;
    std::vector<Task>                     pending_tasks_;
};