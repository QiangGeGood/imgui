# Tech Context

## 开发环境
- OS: Windows 11
- IDE: Visual Studio 2019 (本地已安装)
- 构建: CMake + MSVC
- 包管理: vcpkg (`C:\vcpkg\vcpkg.exe`)，本 demo 不依赖

## 选用后端（demo）
- Platform: Win32（系统自带，无外部依赖）
- Renderer: DirectX 11（Windows SDK 自带）

## 链接库
- d3d11.lib
- dxgi.lib
- d3dcompiler.lib（部分情况非必需，预留）

## 构建方式
```
cmake -S . -B build -G "Visual Studio 16 2019" -A x64
cmake --build build --config Release
build\my_demo\Release\my_demo.exe