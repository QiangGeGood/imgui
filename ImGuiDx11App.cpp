#include "ImGuiDx11App.h"

#include <algorithm>
#include <chrono>
#include <d3d11.h>
#include <stdexcept>
#include <thread>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "Win32Window.h"

namespace {

// "fps <= 0" means "no timed redraws — pure message-driven". This applies
// uniformly to both the active and idle states (e.g. SetIdleFps(0) lets the
// window stop rendering completely when nothing is happening).
constexpr DWORD kWaitForever = INFINITE;
constexpr DWORD kWaitNoSleep = 0;

// Decide how long the UI thread should sleep before its next iteration.
//   fps <= 0    → pure message-driven, no periodic wakeup
//   otherwise   → remainder of this frame's time budget
DWORD ComputeWaitMs(int fps, std::chrono::steady_clock::duration spent)
{
    if (fps <= 0)
        return kWaitForever;

    using namespace std::chrono;
    const auto budget = microseconds(1'000'000 / fps);
    const auto remain = budget - spent;
    if (remain.count() <= 0)
        return kWaitNoSleep;
    return static_cast<DWORD>(duration_cast<milliseconds>(remain).count());
}

} // namespace

ImGuiDx11App::ImGuiDx11App(const std::wstring& title, int width, int height)
    : title_(title), width_(width), height_(height)
{
}

ImGuiDx11App::~ImGuiDx11App()
{
    Stop();
}

void ImGuiDx11App::SetClearColor(float r, float g, float b, float a)
{
    clear_color_[0] = r;
    clear_color_[1] = g;
    clear_color_[2] = b;
    clear_color_[3] = a;
}

void ImGuiDx11App::AddWindow(const std::shared_ptr<UiWindow>& w)
{
    if (!w) return;
    std::lock_guard<std::mutex> lk(windows_mu_);
    ui_windows_.push_back(w);
}

void ImGuiDx11App::Post(Task task)
{
    if (!task) return;
    {
        std::lock_guard<std::mutex> lk(tasks_mu_);
        pending_tasks_.push_back(std::move(task));
    }
    // Wake the UI thread out of MsgWaitForMultipleObjectsEx so the task
    // runs immediately instead of after the current idle timeout.
    if (HWND h = static_cast<HWND>(hwnd_for_stop_.load()))
        ::PostMessageW(h, WM_NULL, 0, 0);
}

void ImGuiDx11App::SetTargetFps(int fps)     { target_fps_.store((fps < 0) ? 0 : fps); }
void ImGuiDx11App::SetIdleFps(int fps)       { idle_fps_.store((fps < 0) ? 0 : fps); }
void ImGuiDx11App::SetIdleTimeoutMs(int ms)  { idle_timeout_ms_.store((ms < 0) ? 0 : ms); }
void ImGuiDx11App::SetVSync(bool on)         { vsync_.store(on); }

void ImGuiDx11App::Start()
{
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true))
        return; // already running
    ui_thread_ = std::thread([this] { UiThreadMain(); });
}

void ImGuiDx11App::Stop()
{
    if (!running_.exchange(false))
    {
        // Not running, but thread may still be joinable if it self-exited.
        if (ui_thread_.joinable()) ui_thread_.join();
        return;
    }
    // Ask the UI thread's window to close from any thread.
    if (HWND h = static_cast<HWND>(hwnd_for_stop_.load()))
        ::PostMessageW(h, WM_CLOSE, 0, 0);
    if (ui_thread_.joinable()) ui_thread_.join();
}

void ImGuiDx11App::Run(const UiCallback& ui)
{
    // Wrap the per-frame UI as a UiWindow so it integrates with the same
    // weak_ptr lifecycle used by AddWindow(). Hold the shared_ptr on the
    // stack until the UI thread joins so the callback stays alive.
    struct CallbackWindow : UiWindow
    {
        UiCallback cb;
        void Draw() override { if (cb) cb(); }
    };
    std::shared_ptr<CallbackWindow> cb_window;
    if (ui)
    {
        cb_window = std::make_shared<CallbackWindow>();
        cb_window->cb = ui;
        AddWindow(cb_window);
    }

    Start();
    if (ui_thread_.joinable()) ui_thread_.join();
}

// ----------------------------------------------------------------------------
// UiThreadMain is the only function that runs on the dedicated UI thread.
// It owns three phases:
//   1) Init     : create the HWND, the D3D11 device/swapchain/RTV, and the
//                 ImGui context + Win32/DX11 backends.
//   2) Run loop : pump messages, react to resize, render one frame, then
//                 sleep until either the next frame deadline or the next
//                 input/posted-task message.
//   3) Shutdown : tear everything down in reverse order; runs even if the
//                 loop threw, thanks to the try/catch that clears running_.
//
// The loop is intentionally simple: it does NOT maintain a long-lived
// "next_frame_time" counter (which previously drifted when the window was
// minimized for a long time). Each iteration measures the time spent
// rendering and waits for the remainder of the current frame budget.
// ----------------------------------------------------------------------------
void ImGuiDx11App::UiThreadMain()
{
    using Clock = std::chrono::steady_clock;

    try
    {
        // ---- 1) Init ------------------------------------------------------
        window_ = std::make_unique<Win32Window>(title_, width_, height_);
        hwnd_for_stop_.store(window_->Handle());
        CreateDeviceAndSwapChain();
        CreateRenderTarget();
        window_->Show();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui_ImplWin32_Init(window_->Handle());
        ImGui_ImplDX11_Init(device_, context_);

        // ---- 2) Run loop --------------------------------------------------
        while (running_.load() && window_->PumpMessages())
        {
            // 2a) Honour any client-area resize that arrived via WM_SIZE.
            UINT new_w = 0, new_h = 0;
            if (window_->ConsumePendingResize(new_w, new_h))
            {
                DestroyRenderTarget();
                swap_chain_->ResizeBuffers(0, new_w, new_h,
                                           DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }

            // 2b) If minimized, skip all rendering and state computation;
            //     just block on messages until the window is restored.
            if (window_->IsMinimized())
            {
                ::MsgWaitForMultipleObjectsEx(0, nullptr, INFINITE,
                                              QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                continue;
            }

            // 2c) Decide effective FPS. A single Clock::now() call serves
            //     both the idle-timeout check and the per-frame timing.
            const auto frame_start = Clock::now();
            const int  target_fps  = target_fps_.load();
            const int  idle_fps    = idle_fps_.load();
            const bool idle        = (frame_start - window_->LastInputTime()) >=
                                     std::chrono::milliseconds(
                                         idle_timeout_ms_.load());
            const int  effective_fps = idle ? idle_fps : target_fps;

            // 2d) Render one frame (effective_fps == 0 means
            //     "message-driven only", no periodic redraw).
            if (effective_fps > 0)
                DrawFrame(nullptr);

            // 2e) Sleep until the next frame deadline OR a new window
            //     message OR Post() poking us with WM_NULL.
            const DWORD wait_ms = ComputeWaitMs(effective_fps,
                                                Clock::now() - frame_start);
            ::MsgWaitForMultipleObjectsEx(0, nullptr, wait_ms,
                                          QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        }

        // ---- 3) Shutdown --------------------------------------------------
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        DestroyRenderTarget();
        DestroyDeviceAndSwapChain();
        hwnd_for_stop_.store(nullptr);
        window_.reset();
    }
    catch (...)
    {
        // Ensure flag is cleared so Stop()/destructor don't deadlock.
    }
    running_.store(false);
}

void ImGuiDx11App::DrawFrame(const UiCallback& per_frame_ui)
{
    // Run any tasks posted from other threads BEFORE NewFrame is fine,
    // but they may want to call ImGui too — so do it AFTER NewFrame.
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Drain posted tasks (one shot each).
    std::vector<Task> tasks;
    {
        std::lock_guard<std::mutex> lk(tasks_mu_);
        tasks.swap(pending_tasks_);
    }
    for (auto& t : tasks) if (t) t();

    // Snapshot live windows while holding the lock; release the lock before
    // calling Draw() so that Draw() may freely call AddWindow() / Post() /
    // release shared_ptrs without re-entering the (non-recursive) mutex.
    std::vector<std::shared_ptr<UiWindow>> alive;
    {
        std::lock_guard<std::mutex> lk(windows_mu_);
        alive.reserve(ui_windows_.size());
        for (auto it = ui_windows_.begin(); it != ui_windows_.end(); )
        {
            if (auto w = it->lock())
            {
                alive.push_back(std::move(w));
                ++it;
            }
            else
            {
                it = ui_windows_.erase(it);
            }
        }
    }
    for (auto& w : alive)
        if (w->IsOpen())
            w->Draw();

    if (per_frame_ui) per_frame_ui();

    ImGui::Render();
    context_->OMSetRenderTargets(1, &rtv_, nullptr);
    context_->ClearRenderTargetView(rtv_, clear_color_);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    swap_chain_->Present(vsync_.load() ? 1u : 0u, 0);
}

void ImGuiDx11App::CreateDeviceAndSwapChain()
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount       = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow      = window_->Handle();
    sd.SampleDesc.Count  = 1;
    sd.Windowed          = TRUE;
    sd.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL fl;
    const D3D_FEATURE_LEVEL fls[] = { D3D_FEATURE_LEVEL_11_0 };
    HRESULT hr = ::D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        fls, 1, D3D11_SDK_VERSION, &sd,
        &swap_chain_, &device_, &fl, &context_);
    if (FAILED(hr))
        throw std::runtime_error("D3D11CreateDeviceAndSwapChain failed");
}

void ImGuiDx11App::DestroyDeviceAndSwapChain()
{
    if (swap_chain_) { swap_chain_->Release(); swap_chain_ = nullptr; }
    if (context_)    { context_->Release();    context_    = nullptr; }
    if (device_)     { device_->Release();     device_     = nullptr; }
}

void ImGuiDx11App::CreateRenderTarget()
{
    ID3D11Texture2D* back_buffer = nullptr;
    swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    device_->CreateRenderTargetView(back_buffer, nullptr, &rtv_);
    back_buffer->Release();
}

void ImGuiDx11App::DestroyRenderTarget()
{
    if (rtv_) { rtv_->Release(); rtv_ = nullptr; }
}