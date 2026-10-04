# 验证记录

本次整理日期：2026-10-04。原压缩目录里的旧 `VALIDATION.md` 记录了此前的模拟验证，但未附带可复现的模拟库；本仓库使用附带的测试源码重新验证，不把旧文档中的通过项作为本次测试结果。

## 验证内容

- `logging_and_options`：默认值和命令行组合；非法数字、溢出、缺值和未知参数拒绝；六种日志等级、源文件 / 函数 / 行号、等级过滤；4500 条消息超过 4096 队列长度后，退出仍完整落盘；初始化失败后的 rm_log 终端兜底。
- `mock_acquisition`：真实应用和 OpenCV，链接测试用模拟 SDK 与独立简化接口。28 项采集场景涵盖 USB3、GigE、Mono8 回退、可选配置失败、跳过首台不支持设备、指定原始设备序号、无设备、SDK 初始化 / 枚举 / 打开 / 必要配置 / 开流失败、超时、坏帧、非法图像、转换 / 归还 / 停流 / 关闭失败。另有 3 项帮助及参数错误检查，确保调用 SDK 前完成参数验证。测试接口不是厂商 ABI，不验证实际 SDK 的二进制兼容性。
- 模拟库记录实际 API 调用，检查取得的帧被归还、退出资源释放顺序正确，失败退出码与日志内容符合预期。
- GitHub Actions：Linux 使用真实 OpenCV 和模拟 SDK 执行以上两组测试；Windows 检查 rm_log 与参数解析，不依赖相机 SDK 和 OpenCV。

## 当前环境限制

当前本地环境为 Windows，提供 MinGW GCC 13.1.0 和 CLion 自带 CMake / Ninja。没有可用的 OpenCV C++ 开发包或已安装的 Galaxy SDK；因此本地不能运行实体相机程序。硬件连接、真实 SDK 动态库兼容性、真实图像转换颜色、GUI 与键盘调参需要按 [SETUP.md](SETUP.md) 在连接相机的环境验收。

## 本次本地执行结果

- Windows / MinGW GCC 13.1.0：`CAMERA_BUILD_DEMO=OFF` 的 CMake 配置、构建成功；CTest 的 `logging_and_options` 通过，0 项失败。
- `tests/mock_sdk.cpp` 使用原厂 SDK 头文件独立编译成功；SDK 的 `GXDef.h` 第 173 行有原始 `typedef was ignored` 警告，未修改厂商文件。
- Python 模拟测试脚本语法编译通过；应用源码未使用 `printf`、`std::cout`、`std::cerr` 或直接 spdlog 等级 API 输出调试信息。
- 本地未执行完整采集测试，完整模拟采集交由仓库 Linux CI 执行；其状态以 [GitHub Actions](https://github.com/chaorenqiang119/camera/actions) 的实际结果为准。

模拟结果始终不等价于实体相机通过。
