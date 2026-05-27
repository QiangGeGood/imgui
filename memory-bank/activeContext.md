# Active Context

## 当前焦点
- 将 `my_demo/main.cpp` 中的窗口创建、D3D11 设备、ImGui 上下文及主循环封装成可复用类。

## 关键决策
- 不修改 ImGui 核心与 examples
- 使用项目自带的 `backends/imgui_impl_win32.cpp` 与 `backends/imgui_impl_dx11.cpp`
- 全部 out-of-source 构建到 `build/`
- 封装拆分成两个类：
  - `Win32Window`：注册/创建/销毁窗口、消息泵、resize 状态；通过 `GWLP_USERDATA` 路由消息到成员函数，零全局变量
  - `ImGuiDx11App`：持有 `std::unique_ptr<Win32Window>`，管理 D3D11 设备/SwapChain/RTV 与 ImGui 上下文，提供 `Run(std::function<void()>)` 注入 UI 回调
- D3D 接口仍用裸指针 + `Release()`（COM 风格），不引入 ComPtr 以保持依赖最小化
- 构造失败抛 `std::runtime_error`

- 进一步把每个 ImGui 窗口拆为独立类，由 `ImGuiDx11App` 集中管理：
  - 基类 `UiWindow`（纯虚 `Draw()`，含 `open_` 与 `OpenFlag()`）
  - 派生类示例：`HelloWindow`（counter）、`StatsWindow`（FPS/帧时）、`ProgressWindow`（动画进度条，可暂停/重置/调速）
  - `ImGuiDx11App::AddWindow(const std::shared_ptr<UiWindow>&)` 注册，内部存为 `weak_ptr`
  - **App 不拥有窗口（容器为 `vector<weak_ptr<UiWindow>>`）**：创建方必须长期持有 `shared_ptr`，否则窗口注册即析构
  - 优势：模块析构（其持有的 shared_ptr 释放）→ 窗口立即销毁 → 下一帧 App 自动剔除过期项（`lock()` 失败即 `erase`），实现"插件即生命周期"模型，无需显式 `RemoveWindow`
  - `Run(UiCallback = nullptr)` 仍可接受 lambda，便于临时 UI

## 文件
- **根目录（框架公共 API，便于其他模块复用）**：
  - `Win32Window.h` / `.cpp`
  - `ImGuiDx11App.h` / `.cpp`
  - `UiWindow.h`
- **my_demo/（demo 专属）**：
  - `HelloWindow.h` / `.cpp`
  - `StatsWindow.h` / `.cpp`
  - `ProgressWindow.h` / `.cpp`
  - `main.cpp`（注册常驻窗口 + `Run(lambda)` 内"Lifetime Demo"面板）
  - `CMakeLists.txt` 通过 `${CMAKE_SOURCE_DIR}/...` 引用根目录的框架源文件（方案 A2：直接编入，不抽 lib）
- 根目录已在 `imgui` target 的 PUBLIC include 路径中，故 `#include "ImGuiDx11App.h"` 等无需任何路径前缀

## 下一步
- 如需要：扩展示例 UI、添加更多窗口/资源
