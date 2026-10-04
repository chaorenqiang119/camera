# 大恒工业相机采集与 rm_log 日志

使用 Galaxy C API 调用大恒 USB3 / GigE 工业相机，以 OpenCV 显示画面，并统一使用现有 `rm_log` 封装输出调试信息。默认自动打开第一台支持的相机，连续采集；按 ESC、关闭窗口或 Ctrl+C 退出。

本项目对应的要求：复现相机环境配置与调用流程、将调试信息替换为 `RM_LOG_*` 格式、将项目代码提交到 GitHub。实体相机验收步骤见 [配置与运行](docs/SETUP.md)，本次验证范围见 [验证记录](docs/VALIDATION.md)。

原项目已有的相机运行日志已归档到 [docs/logs](docs/logs/README.md)，保留无设备失败及两次采集 1 帧后正常退出的原始记录。

## 项目结构

```text
camera/
├── CMakeLists.txt             # SDK / OpenCV 查找与构建
├── src/main.cpp              # 初始化、枚举、配置、采集、调参和资源释放
├── include/options.h         # 运行参数解析
├── util/rm_log/              # 现有日志封装及初始化失败兜底
├── util/spdlog/              # 原项目附带的 spdlog 1.12.0 / fmt
├── tests/                    # 日志、参数、独立模拟接口及异常路径测试
├── docs/                     # 环境复现、验收与验证记录
└── .github/workflows/ci.yml  # Linux 模拟采集与 Windows 核心检查
```

## 配置与运行

推荐先在原项目使用的 Ubuntu 环境复现。需要 C++17、CMake 3.16+、OpenCV（core / imgproc / highgui）和安装好的大恒 Galaxy SDK。`rm_log`、spdlog 和 fmt 已包含在仓库中。厂商 SDK 头文件、动态库和相机驱动需从官方安装包获取，下载入口与 Windows 配置见 [SETUP.md](docs/SETUP.md)。

```bash
git clone https://github.com/chaorenqiang119/camera.git
cd camera
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
./build/camera_demo
```

如果 SDK 没有安装到系统搜索路径：

```bash
cmake -S . -B build -DGALAXY_SDK_ROOT=/path/to/GalaxySDK
```

CLion 打开仓库根目录的 `CMakeLists.txt`，选择 `camera_demo` 目标；工作目录设为仓库根目录。连接相机并关闭占用相机的 GalaxyView，再运行。

## 采集流程与默认配置

1. `GXInitLib` 初始化 SDK，`GXUpdateAllDeviceList` 枚举设备。
2. 检查设备类型并使用 SDK 原始序号打开第一台 USB3 / GigE 相机。
3. 连续采集、关闭触发；尝试关闭自动曝光 / 自动增益、设置连续自动白平衡和带宽上限；配置 5 个采集缓存。
4. 优先设置 BayerRG8，不支持时回退 Mono8；`GXStreamOn` 开流。
5. `GXDQBuf` 取帧，检查状态、尺寸和数据长度；使用 `DxRaw8toRGB24Ex` 转换 Bayer 图像，Mono8 复制数据；`GXQBuf` 归还帧后显示。
6. 退出或异常时，按归还帧、停流、关闭设备、关闭 SDK 的顺序释放资源。

必要配置失败会退出；型号不支持白平衡等可选节点时记录 WARN 并继续。连续 3 次超时或 10 张无效帧会退出。程序不改写持久化 UserSet，曝光和增益保留设备当前值，可通过键盘调整。

| 按键 | 操作 |
|---|---|
| e / d | 曝光时间增加 / 减少 25 μs |
| a / q | 增益增加 / 减少 0.1，单位以相机节点为准 |
| s / w | Gamma 增加 / 减少 0.1 |
| ESC / Ctrl+C / 关闭窗口 | 停止采集并释放资源 |

调参会读取节点范围及步长并限幅。不支持 Gamma 等节点时记录 WARN。

## 运行参数

```bash
./build/camera_demo --help
./build/camera_demo --no-display --frames 100
./build/camera_demo --camera-index 2 --log-level trace --log-file logs/camera2.log
```

| 参数 | 默认值 / 含义 |
|---|---|
| `--no-display` | 关闭显示，仍执行采集与图像转换 |
| `--frames N` | 采集 N 张有效帧后退出，默认 0 表示持续采集 |
| `--camera-index N` | SDK 中从 1 开始的设备序号，默认自动选择 |
| `--log-file PATH` | 默认 `logs/daheng.log`，相对于运行工作目录 |
| `--log-level LEVEL` | 默认 debug；支持 trace / debug / info / warn / error / critical |
| `--help` | 输出帮助，不调用相机 SDK |

退出码：0 为正常结束，1 为初始化 / 采集 / 资源释放失败，2 为运行参数错误。

## rm_log 格式

所有应用日志，包括启动、SDK 状态码与错误详情、配置、超时、坏帧、转换失败、FPS、调参、退出以及日志初始化失败，都经过 `RM_LOG_*` 宏。

```cpp
INIT_LOG("logs/daheng.log", "debug", "trace", "debug");
RM_LOG_INFO("Opened camera, SDK index={}", camera_index);
RM_LOG_DEBUG("FPS={:.2f}, total frames={}", fps, total_frames);
RM_LOG_WARN("Invalid frame id={}, status={}", frame_id, status);
RM_LOG_ERROR("{} failed: status={}, detail={}", operation, status, detail);
```

格式为 `[时间] [等级] [thread 线程号] [源文件 函数:行号] 内容`。正常日志采用异步队列 4096、单工作线程、阻塞溢出策略；单个文件上限 5 MiB，保留 3 个轮转文件；WARN 及以上触发刷新，退出时排空队列。默认记录 DEBUG 及以上，`--log-level trace` 开启逐帧日志。初始化前及初始化失败时，封装内使用同步终端日志保持相同格式。

## 可重复验证

不依赖相机和 OpenCV，检查参数解析与 rm_log：

```bash
cmake -S . -B build-core -DCAMERA_BUILD_DEMO=OFF
cmake --build build-core --parallel 2
ctest --test-dir build-core --output-on-failure
```

安装 OpenCV 后，使用显式的模拟 SDK 检查采集流程与异常释放：

```bash
cmake -S . -B build-mock -DCAMERA_USE_MOCK_SDK=ON
cmake --build build-mock --parallel 2
ctest --test-dir build-mock --output-on-failure
```

模拟模式会在配置和运行时明确警告，默认关闭。测试接口的结构和常量是独立简化定义，不是厂商 SDK 的 ABI。模拟测试不能证明实体相机、厂商动态库、颜色转换或 GUI 调参已经通过验收；真实运行使用独立的 `build` 目录和 `CAMERA_USE_MOCK_SDK=OFF`。

第三方来源与许可说明见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
