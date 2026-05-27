# Product Context

## Dear ImGui 是什么
- 立即模式（Immediate Mode）GUI 库，C++ 编写，无外部依赖
- 用于游戏、工具、3D/2D 应用的调试与可视化界面
- 自身不创建窗口、不直接绘制；产出 DrawData，由 Backend 接入用户应用

## 架构两层
1. **Platform Backend**：处理窗口、输入、剪贴板（如 Win32 / GLFW / SDL）
2. **Renderer Backend**：把 DrawData 实际绘制出来（如 DX11 / OpenGL3 / Vulkan）

## 关键源文件
- `imgui.cpp / imgui.h`：核心 API
- `imgui_draw.cpp`：绘制原语
- `imgui_widgets.cpp`：按钮、滑块等控件
- `imgui_tables.cpp`：表格
- `imgui_demo.cpp`：官方 ShowDemoWindow 大全
- `imconfig.h`：用户可调编译宏
- `backends/`：各平台与渲染器后端
- `examples/`：每种组合一个示例工程