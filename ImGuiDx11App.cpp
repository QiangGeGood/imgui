#include "ImGuiDx11App.h"

#include <d3d11.h>
#include <stdexcept>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

ImGuiDx11App::ImGuiDx11App(const std::wstring& title, int width, int height)
    : window_(std::make_unique<Win32Window>(title, width, height))
{
    CreateDeviceAndSwapChain();
    CreateRenderTarget();

    window_->Show();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(window_->Handle());
    ImGui_ImplDX11_Init(device_, context_);
}

ImGuiDx11App::~ImGuiDx11App()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    DestroyRenderTarget();
    DestroyDeviceAndSwapChain();
    // window_ is destroyed by unique_ptr after this point.
}

void ImGuiDx11App::SetClearColor(float r, float g, float b, float a)
{
    clear_color_[0] = r;
    clear_color_[1] = g;
    clear_color_[2] = b;
    clear_color_[3] = a;
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

void ImGuiDx11App::AddWindow(const std::shared_ptr<UiWindow>& w)
{
    if (w) ui_windows_.push_back(w); // implicit shared_ptr -> weak_ptr
}

void ImGuiDx11App::Run(const UiCallback& ui)
{
    while (window_->PumpMessages())
    {
        UINT new_w = 0, new_h = 0;
        if (window_->ConsumePendingResize(new_w, new_h))
        {
            DestroyRenderTarget();
            swap_chain_->ResizeBuffers(0, new_w, new_h, DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Draw live windows; drop expired ones (caller released its shared_ptr).
        for (auto it = ui_windows_.begin(); it != ui_windows_.end(); )
        {
            if (auto w = it->lock())
            {
                if (w->IsOpen())
                    w->Draw();
                ++it;
            }
            else
            {
                it = ui_windows_.erase(it);
            }
        }

        if (ui) ui();

        ImGui::Render();
        context_->OMSetRenderTargets(1, &rtv_, nullptr);
        context_->ClearRenderTargetView(rtv_, clear_color_);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        swap_chain_->Present(1, 0);
    }
}
