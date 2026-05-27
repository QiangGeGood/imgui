#include "Win32Window.h"
#include <stdexcept>

#include "imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

Win32Window::Win32Window(const std::wstring& title, int width, int height)
    : class_name_(L"MyImGuiDemoWindow")
    , hinstance_(::GetModuleHandleW(nullptr))
{
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, &Win32Window::StaticWndProc, 0, 0,
                       hinstance_, nullptr, nullptr, nullptr, nullptr,
                       class_name_.c_str(), nullptr };
    if (!::RegisterClassExW(&wc))
        throw std::runtime_error("RegisterClassExW failed");

    hwnd_ = ::CreateWindowExW(0, class_name_.c_str(), title.c_str(),
                              WS_OVERLAPPEDWINDOW, 100, 100, width, height,
                              nullptr, nullptr, hinstance_, this);
    if (!hwnd_)
    {
        ::UnregisterClassW(class_name_.c_str(), hinstance_);
        throw std::runtime_error("CreateWindowExW failed");
    }
}

Win32Window::~Win32Window()
{
    if (hwnd_)      ::DestroyWindow(hwnd_);
    if (hinstance_) ::UnregisterClassW(class_name_.c_str(), hinstance_);
}

void Win32Window::Show(int cmd_show)
{
    ::ShowWindow(hwnd_, cmd_show);
    ::UpdateWindow(hwnd_);
}

bool Win32Window::PumpMessages()
{
    MSG msg;
    while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
        if (msg.message == WM_QUIT)
            return false;
    }
    return true;
}

bool Win32Window::ConsumePendingResize(UINT& out_w, UINT& out_h)
{
    if (pending_w_ == 0 || pending_h_ == 0)
        return false;
    out_w = pending_w_;
    out_h = pending_h_;
    pending_w_ = pending_h_ = 0;
    return true;
}

LRESULT CALLBACK Win32Window::StaticWndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    // Bind 'this' on creation via CREATESTRUCT::lpCreateParams.
    if (m == WM_NCCREATE)
    {
        auto* cs   = reinterpret_cast<CREATESTRUCTW*>(l);
        auto* self = reinterpret_cast<Win32Window*>(cs->lpCreateParams);
        ::SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->hwnd_ = h;
    }

    auto* self = reinterpret_cast<Win32Window*>(::GetWindowLongPtrW(h, GWLP_USERDATA));
    if (self)
        return self->HandleMessage(h, m, w, l);
    return ::DefWindowProcW(h, m, w, l);
}

LRESULT Win32Window::HandleMessage(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (ImGui_ImplWin32_WndProcHandler(h, m, w, l))
        return true;

    switch (m)
    {
    case WM_SIZE:
        if (w != SIZE_MINIMIZED)
        {
            pending_w_ = LOWORD(l);
            pending_h_ = HIWORD(l);
        }
        return 0;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(h, m, w, l);
}