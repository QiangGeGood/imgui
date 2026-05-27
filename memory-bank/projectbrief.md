# Project Brief — Dear ImGui (本地工作区)

## 目标
本工作区是开源库 **Dear ImGui** 的源码副本（fork: QiangGeGood/imgui）。当前任务：熟悉项目并编写一个简单的可运行 demo。

## 范围
- 不修改 ImGui 核心源码
- 在项目根目录下新增 `my_demo/` 子目录，作为自定义示例
- 使用 CMake + out-of-source 构建（build/ 目录）
- 不污染原有 `examples/` 与 `backends/` 内容