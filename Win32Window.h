// Win32Window: lightweight RAII wrapper around a Win32 top-level window.

#pragma once

#include <Windows.h>
#include <string>

class Win32Window
{
public:
    Win32Window(const std::wstring& title, int width, int height);
    ~Win32Window();

    Win32Window(const Win32Window&) = delete;
    Win32Window& operator=(const Win32Window&) = delete;

    HWND Handle() const { return hwnd_; }

    void Show(int cmd_show = SW_SHOWDEFAULT);

    // Pump all pending messages. Returns false if a WM_QUIT was received.
    bool PumpMessages();

    // If a resize is pending, copy the new client size and clear the flag.
    // Returns true when a resize is pending.
    bool ConsumePendingResize(UINT& out_w, UINT& out_h);

private:
    static LRESULT CALLBACK StaticWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(HWND, UINT, WPARAM, LPARAM);

    std::wstring class_name_;
    HINSTANCE    hinstance_ = nullptr;
    HWND         hwnd_      = nullptr;
    UINT         pending_w_ = 0;
    UINT         pending_h_ = 0;
};