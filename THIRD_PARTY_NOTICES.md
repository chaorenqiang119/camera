# 第三方来源说明

- `util/spdlog/` 来自原项目附带的 spdlog 1.12.0，头文件标注 Gabi Melman 与贡献者版权，使用 MIT 许可。对应许可原文见 `util/spdlog/LICENSE`，上游为 [gabime/spdlog v1.12.0](https://github.com/gabime/spdlog/tree/v1.12.0)。
- spdlog 内置 fmt 的许可保留在 `util/spdlog/fmt/bundled/fmt.license.rst`，遵循其中的版权和许可声明。
- DAHENG IMAGING Galaxy 头文件、库和驱动不公开再分发。使用者从 [大恒官方网站](https://en.daheng-imaging.com/list-59-1.html) 获取 SDK 并遵循厂商许可。原项目附带的厂商头文件仅保留在本地，不纳入 Git 提交。
- `tests/mock_include/` 是本项目独立编写的简化测试接口，结构与常量不兼容厂商 ABI，仅用于模拟控制流程；没有提供厂商实现或图像处理算法。
- `util/rm_log/` 保留原项目的封装 API 与日志格式，补充了初始化失败兜底、非法等级检查和退出时异步日志排空。
- OpenCV 通过系统安装查找，不包含其二进制或源码。来源：[OpenCV](https://opencv.org/)。

本仓库不为上述第三方内容另行指定许可证。
