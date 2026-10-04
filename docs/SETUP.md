# 环境复现与实体相机验收

## 原项目提供的环境线索

旧 CMake 缓存来自 Linux，SDK 库路径为 `/usr/lib/libgxiapi.so`，OpenCV 配置位于 `/usr/local/lib/cmake/opencv4`。仓库不提交该缓存、原机器绝对路径、旧可执行文件或日志，克隆后应重新配置。

原项目的本地 `GxIAPI.h` 标注 C API 版本 `2.0.2412.9191`，`DxImageProc.h` 标注 `1.0.2405.9251`；这些厂商文件保留在本地，不公开再分发。仓库使用已安装 SDK 的配套头文件；SDK 版本需支持本项目使用的字符串节点接口和 `DxRaw8toRGB24Ex`。CMake 在真实模式中执行编译链接检查，发现不匹配会提前报错。

## Ubuntu

1. 从 [大恒官方下载页](https://en.daheng-imaging.com/list-59-1.html) 获取适合系统架构和相机接口的 Galaxy SDK，按安装包内说明安装 SDK、驱动和设备权限规则。不要仅复制头文件。
2. 安装工具与 OpenCV：

   ```bash
   sudo apt-get update
   sudo apt-get install -y build-essential cmake libopencv-dev
   ```

3. 在 GalaxyView 中确认相机可以打开并显示画面，然后关闭 GalaxyView，避免设备被独占。
4. 编译运行：

   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCAMERA_USE_MOCK_SDK=OFF
   cmake --build build --parallel 2
   ./build/camera_demo
   ```

自定义安装路径可以使用 `GALAXY_SDK_ROOT`，也可以明确指定：

```bash
cmake -S . -B build \
  -DGXIAPI_INCLUDE_DIR=/path/to/sdk/include \
  -DGXIAPI_LIBRARY=/path/to/sdk/lib/libgxiapi.so \
  -DOpenCV_DIR=/path/to/opencv/lib/cmake/opencv4
```

部分 SDK 将图像处理符号导出在 `libgxiapi.so` 中，另一些提供独立库。需要独立库时再加 `-DDXIMAGEPROC_LIBRARY=/path/to/libdximageproc.so`。运行时确保动态链接器能找到 SDK 和 OpenCV 库；按 SDK 安装说明配置，或临时设置相应目录的 `LD_LIBRARY_PATH`。

USB3 相机接 USB3 端口；权限不足时检查厂商安装的设备规则。GigE 相机与网卡应处于匹配的网段，先用厂商工具验证可达性及流传输。项目仅自动支持 USB3 / GigE。

## Windows

安装 Galaxy Windows SDK 和相机驱动，安装与编译器匹配的 OpenCV C++ 开发包。建议使用 Visual Studio 2022 x64；厂商 `.lib` 与 OpenCV 库应和工具链、架构匹配。仅有 Python 的 `cv2` 不能替代 C++ 开发包。

在 PowerShell 中执行，路径按实际安装位置修改：

```powershell
cmake -S . -B build -A x64 `
  -DGXIAPI_INCLUDE_DIR="C:/path/to/GalaxySDK/Development/Include" `
  -DGXIAPI_LIBRARY="C:/path/to/GalaxySDK/Development/Lib/x64/GxIAPI.lib" `
  -DDXIMAGEPROC_LIBRARY="C:/path/to/GalaxySDK/Development/Lib/x64/DxImageProc.lib" `
  -DOpenCV_DIR="C:/path/to/opencv/build"
cmake --build build --config Debug --parallel 2
.\build\Debug\camera_demo.exe --no-display --frames 100
```

SDK 实际目录布局可能不同，应以安装包内的开发示例为准。确保厂商及 OpenCV 的 DLL 目录在当前终端的 `PATH` 中。Windows 的 SDK 帧缓冲区地址在附带头文件中是 64 位整数，代码已显式转换为指针。

## 实体相机验收步骤

1. 使用真实模式构建，运行日志应没有 `MOCK SDK enabled`。
2. 连接相机，运行 `camera_demo --no-display --frames 100`。应枚举并打开设备，采集 100 张有效帧，按顺序释放资源并以 0 退出。
3. 默认运行，确认实际画面、色彩与 FPS；逐一检查 e / d、a / q 和支持时的 s / w，日志应显示参数变化。
4. 分别使用 ESC、窗口关闭和 Ctrl+C 退出，随后重新运行确认设备不被上一进程占用。
5. 拔掉相机后启动，应记录无设备或 SDK 错误并以 1 退出；检查 `logs/daheng.log` 中的等级、源文件、函数和行号。
6. 保存实际相机型号、序列号（必要时脱敏）、系统、SDK / OpenCV 版本、运行命令和日志摘要，作为硬件验收记录。当前仓库没有将这些待验证项标记为通过。
