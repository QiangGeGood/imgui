# System Patterns

## Demo 结构
```
my_demo/
├── CMakeLists.txt   # imgui 静态库 + my_demo 可执行
└── main.cpp         # Win32 窗口 + DX11 + ImGui 主循环
```

## CMake 目标
- `imgui` (STATIC LIBRARY)
  - sources: imgui.cpp, imgui_draw.cpp, imgui_tables.cpp, imgui_widgets.cpp, imgui_demo.cpp,
            backends/imgui_impl_win32.cpp, backends/imgui_impl_dx11.cpp
  - include: 项目根目录、backends/
- `my_demo` (WIN32 EXECUTABLE)
  - sources: my_demo/main.cpp
  - link: imgui, d3d11, dxgi

## ImGui 接入三步
1. `ImGui::CreateContext()` + 配置 IO
2. 后端初始化：`ImGui_ImplWin32_Init(hwnd)` + `ImGui_ImplDX11_Init(dev, ctx)`
3. 每帧：NewFrame → 构建 UI → Render → 后端 RenderDrawData → Swap Present

## 退出顺序
ImGui_ImplDX11_Shutdown → ImGui_ImplWin32_Shutdown → ImGui::DestroyContext → 释放 DX 资源