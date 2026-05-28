# Active Context

## 当前焦点
- 将 `my_demo/main.cpp` 中的窗口创建、D3D11 设备、ImGui 上下文及主循环封装成可复用类。
- 支持"嵌入式可视化"：外部程序已有自己的消息循环时，最小侵入接入 ImGuiDx11App。

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

## 嵌入模式（embed mode）
- `ImGuiDx11App` 不再阻塞构造方线程；公共 API：
  - `Start()`：启动专用 UI 线程，**在该线程内**创建 HWND / D3D11 / ImGui 上下文并跑消息泵；非阻塞返回
  - `Stop()`：从任意线程 `PostMessage(WM_CLOSE)` 让 UI 线程退出并 join；析构函数自动调用
  - `IsRunning()`：UI 线程是否在跑（窗口被关闭后变 false）
  - `Post(task)`：把一段代码丢到 UI 线程下一帧执行（必须线程安全地与 ImGui 交互）
  - `AddWindow(shared_ptr<UiWindow>)`：任意线程任意时刻可调（mutex 保护）
  - `Run(callback = nullptr)`：保留阻塞模式（= `Start()` + `join()`），demo 不变
- 线程安全约束：**ImGui/D3D 调用必须在 UI 线程**（即 `UiWindow::Draw()` / `Post` 任务）；外部线程只负责写共享数据（atomic / mutex / 快照）
- 关键内部细节：`hwnd_for_stop_` 是 `atomic<void*>`，由 UI 线程创建窗口后发布；`Stop()` 不再读 `window_`（避免跨线程 race）
- **重入陷阱（已修复）**：`DrawFrame` 中绘制窗口必须先在持锁状态下把 `weak_ptr` 提升为 `shared_ptr` 快照，**释放锁后再 `Draw()`**；否则 `UiWindow::Draw()` 内若调用 `AddWindow()`（如 `Run(lambda)` 中用按钮新建窗口），会对非递归 `windows_mu_` 二次加锁，触发未定义行为/崩溃。同理 `Post()` 任务在锁外执行也是同一原因

## 示例
- `my_demo/` 原 demo 保留，使用 `Run(lambda)` 阻塞模式
- `embed_demo/`（新增）：模拟"外部已有 main 循环"的场景，仅 3 行接入：
  ```cpp
  ImGuiDx11App ui(L"Embedded Debug UI", 480, 260);
  ui.AddWindow(std::make_shared<StateWindow>(&state));
  ui.Start();
  while (ui.IsRunning()) { /* 原有循环 */ }
  ```
  通过 `std::atomic` 共享状态，UI 线程实时绘制

## 帧率控制 / CPU 节流（新增）

### 问题
原主循环 `while(PumpMessages()) DrawFrame()` 是忙轮询，即使 VSync 限制 GPU 帧率，CPU 端 ImGui draw-list 仍每帧执行（~60 FPS），空闲时 CPU 占用高。

### 方案（已实现：消息驱动 + 空闲降帧 + VSync 开关）
- `Win32Window` 增加 `LastInputTime()`（记录最近输入消息时间戳）和 `IsMinimized()`
- `ImGuiDx11App` 新增 4 个配置接口：
  - `SetTargetFps(int)` — 活跃时帧率上限（默认 60；0 = 不限）
  - `SetIdleFps(int)` — 空闲时帧率（默认 10；0 = 仅消息驱动不渲染）
  - `SetIdleTimeoutMs(int)` — 多久无输入视为空闲（默认 500ms）
  - `SetVSync(bool)` — Present VSync 开关（默认 on）
- 主循环改用 `MsgWaitForMultipleObjectsEx`：等待"下一帧 deadline"或"窗口消息"或 `Post()` 发来的 `WM_NULL`， whichever comes first
- 最小化 → 一律 `wait_ms = INFINITE`，完全跳过渲染
- 空闲且 `idle_fps == 0` → 同样 `INFINITE` 等待消息
- `Present(vsync ? 1 : 0, 0)` 根据 VSync 标志切换
- `Post()` 里有新任务时 `PostMessage(WM_NULL)` 唤醒 UI 线程

### UiThreadMain 结构（三段式）
1. **Init**：HWND → D3D11 设备/SwapChain/RTV → ImGui 上下文
2. **Run loop**：每次迭代 4 个子步骤
   - 2a 处理 resize
   - 2b 判定 `minimized` / `idle` / `effective_fps`
   - 2c 渲染一帧（除非 `minimized` 或 `effective_fps == 0`）
   - 2d `ComputeWaitMs()` + `MsgWaitForMultipleObjectsEx`：按本帧实际耗时计算等待（不再累加全局 `next_frame_time`）
3. **Shutdown**：反向销毁；`try/catch` 保证 `running_` 必被清除

### ComputeWaitMs（匿名命名空间小函数，2 参数）
- `fps <= 0` → `kWaitForever`（纯消息驱动）
- 帧预算有剩余 → 返回剩余毫秒
- 帧预算耗尽 → `kWaitNoSleep`（立即下一轮）
- 统一语义：`fps == 0` 在 active 和 idle 状态下都意味着"消息驱动，不周期渲染"
- `minimized` 参数已移除：最小化由主循环 2b 步 `continue` 早退处理，不再进入 ComputeWaitMs

### 主循环优化要点
- **2b 最小化早退**：`IsMinimized()` 时直接 `MsgWaitForMultipleObjectsEx(INFINITE)` + `continue`，跳过后续所有状态计算和渲染
- **2c 单次 `Clock::now()`**：`frame_start` 同时用于 idle 判定和帧耗时计算，减少 `QueryPerformanceCounter` 调用

### 效果预期
- 空闲（无操作 500ms 后）：~10 FPS → CPU 接近 0%
- 活跃交互：与原来体验一致（60 FPS）
- 最小化：完全不渲染，CPU = 0%；恢复后立即按 `target_fps` 渲染

## 下一步
- 如需要：扩展示例 UI、添加更多窗口/资源
