# Progress

## 已完成
- 阅读 examples/example_win32_directx11/main.cpp，理解 ImGui 接入流程
- 初始化 memory-bank 文档
- 创建 my_demo/main.cpp（Win32 + DX11，含自定义控件 + 官方 demo 切换）
- 创建 my_demo/CMakeLists.txt
- 创建顶层 CMakeLists.txt（聚合 imgui 静态库 + my_demo）
- cmake 配置（VS2019, x64）成功
- 构建 Release 成功，产物：`build/my_demo/Release/my_demo.exe`
- 精简 demo 到最小功能：main.cpp 仅 ~110 行；UI 仅 "Hello + 一个按钮 + 计数"；移除 imgui_demo.cpp 编译；exe 体积 929KB → 385KB
- 封装 demo 框架为可复用类：
  - `Win32Window`（窗口注册/创建/消息泵/resize），使用 GWLP_USERDATA 路由消息，消除原全局变量
  - `ImGuiDx11App`（D3D11 设备/SwapChain/RTV + ImGui 上下文 + 主循环），通过 `Run(std::function<void()>)` 注入 UI 回调
  - `main.cpp` 精简为 ~20 行（仅 lambda 描述 UI）
  - Release 构建通过
- 按 UiWindow 接口拆分各 ImGui 窗口：
  - 新增 `UiWindow`（基类）/ `HelloWindow` / `StatsWindow`
  - `ImGuiDx11App::AddWindow(const std::shared_ptr<UiWindow>&)`，内部存 `vector<weak_ptr<UiWindow>>`
  - **App 不拥有窗口**：创建方释放 → 窗口析构 → App 下一帧 lock 失败自动 erase
  - `main.cpp` 用 `std::make_shared` 创建并持有到 `Run()` 结束
  - Release 构建通过
- 新增 `ProgressWindow`（动画进度条 + 暂停/重置/调速）+ `main.cpp` "Lifetime Demo" 面板：
  - 在 `Run(lambda)` 中通过按钮动态创建/销毁 ProgressWindow 的 shared_ptr
  - 直观验证：销毁 shared_ptr → 窗口立刻消失（析构），App 下一帧自动 erase 过期 weak_ptr
  - Release 构建通过
- 将框架公共头与实现移至项目根目录，方便其他模块复用（方案 A2）：
  - 根目录：`Win32Window.h/.cpp`、`ImGuiDx11App.h/.cpp`、`UiWindow.h`
  - my_demo/ 仅保留 demo 专属（main + 三个派生 UiWindow）
  - `my_demo/CMakeLists.txt` 通过 `${CMAKE_SOURCE_DIR}/...` 直接编入框架 .cpp（未抽静态库）
  - 根目录已在 imgui PUBLIC include 路径，include 写法无需路径前缀
  - Release 构建通过
- ImGuiDx11App 升级为可后台运行（embed mode），便于嵌入已有消息循环：
  - 新增 `Start()` / `Stop()` / `IsRunning()` / `Post()`；HWND + D3D + ImGui 上下文均在 UI 线程内创建
  - `AddWindow()` 改为线程安全（mutex 保护 weak_ptr 容器）；`pending_tasks_` 同上
  - `Stop()` 通过 `atomic<void*> hwnd_for_stop_` 跨线程 `PostMessage(WM_CLOSE)`，析构自动 Stop+join
  - 旧 `Run(callback)` 保留为兼容入口（= Start + 把回调包成 CallbackWindow 走 weak_ptr 路径 + join）
  - 新增 `embed_demo/`（控制台循环 + atomic 共享状态 + 仅 3 行接入 UI），Release 构建通过

## 运行
- 双击 exe 或 `build\my_demo\Release\my_demo.exe`

## 已知问题
- 无

## 后续可扩展
- 加入字体加载、Docking branch、其他 Renderer Backend 等
