#include <cstddef>
#include "rm_log.h"
#include "options.h"
#include <GxIAPI.h>
#include <DxImageProc.h>
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <stdexcept>
#include <string>
#include <vector>

volatile std::sig_atomic_t stop_requested = 0;
void Stop(int) { stop_requested = 1; }

// Galaxy 的 Windows 头文件用 64 位整数保存缓冲区地址，Linux 使用 void*。
void* FrameData(const GX_FRAME_BUFFER& frame) {
#ifdef _WIN32
    return reinterpret_cast<void*>(static_cast<uintptr_t>(frame.pImgBuf));
#else
    return frame.pImgBuf;
#endif
}

// 必要操作失败就停止；可选参数失败只记录警告。
bool CheckGX(GX_STATUS status, const char* operation, bool required = true) {
    if (status == GX_STATUS_SUCCESS) return true;
    std::size_t size = 0;
    std::string detail = "SDK error description unavailable";
    if (GXGetLastError(nullptr, nullptr, &size) == GX_STATUS_SUCCESS && size > 0) {
        std::vector<char> text(size + 1, '\0');
        if (GXGetLastError(nullptr, text.data(), &size) == GX_STATUS_SUCCESS) detail = text.data();
    }
    if (required) {
        RM_LOG_ERROR("{} failed: status={}, detail={}", operation, status, detail);
        throw std::runtime_error(operation);
    }
    RM_LOG_WARN("{} failed: status={}, detail={}", operation, status, detail);
    return false;
}

// 用完一帧后归还 SDK，避免缓存被耗尽。
bool ReturnFrame(GX_DEV_HANDLE device, PGX_FRAME_BUFFER& frame, bool required = true) {
    PGX_FRAME_BUFFER saved = frame;
    frame = nullptr;
    return CheckGX(GXQBuf(device, saved), "GXQBuf", required);
}

// 按键调参：读当前值 -> 修改 -> 限幅 -> 写回。
void AdjustParameter(GX_DEV_HANDLE device, const char* name, double delta) {
    GX_FLOAT_VALUE value{};
    if (!CheckGX(GXGetFloatValue(device, name, &value), name, false)) return;
    double next = std::clamp(value.dCurValue + delta, value.dMin, value.dMax);
    if (value.bIncIsValid && value.dInc > 0) {
        next = value.dMin + std::round((next - value.dMin) / value.dInc) * value.dInc;
        next = std::clamp(next, value.dMin, value.dMax);
    }
    if (CheckGX(GXSetFloatValue(device, name, next), name, false))
        RM_LOG_INFO("{}: {} -> {}", name, value.dCurValue, next);
}

int main(int argc, char* argv[]) {
    Options options;
    try {
        options = ParseOptions(argc, argv);
    } catch (const std::exception& e) {
        RM_LOG_ERROR("Invalid arguments: {}; use --help", e.what());
        utils::RMLOG::instance().Close();
        return 2;
    }
    if (options.help) {
        RM_LOG_INFO("{}", kUsage);
        utils::RMLOG::instance().Close();
        return 0;
    }
    const bool show_image = options.show_image;
    const uint64_t frame_limit = options.frame_limit;
    try {
        INIT_LOG(options.log_file, options.log_level, "trace", options.log_level);
    } catch (const std::exception& e) {
        // 初始化失败时由 rm_log 内的终端日志兜底，仍保持相同格式。
        RM_LOG_ERROR("Cannot initialize rm_log: {}", e.what());
        utils::RMLOG::instance().Close();
        return 1;
    }

    GX_DEV_HANDLE device = nullptr;
    PGX_FRAME_BUFFER frame = nullptr;
    bool library_ready = false, streaming = false, window_ready = false;
    int result = 0;
    std::signal(SIGINT, Stop);
    std::signal(SIGTERM, Stop);
    try {
        // 1. 初始化大恒 SDK。
        RM_LOG_INFO("Program started: display={}, frame limit={}, log={}",
                    show_image, frame_limit, options.log_file);
#ifdef CAMERA_MOCK_SDK
        RM_LOG_WARN("MOCK SDK enabled: simulated frames; no physical camera is accessed");
#endif
        CheckGX(GXInitLib(), "GXInitLib");
        library_ready = true;

        // 2. 自动打开第一台 USB3 或 GigE 相机，不需要填写序列号。
        uint32_t count = 0, camera_index = 0;
        CheckGX(GXUpdateAllDeviceList(&count, 1000), "GXUpdateAllDeviceList");
        RM_LOG_INFO("Found {} camera(s)", count);
        if (count == 0) throw std::runtime_error("No camera found: check connection and permissions");
        if (options.camera_index > count) throw std::runtime_error("Camera index exceeds device count");
        for (uint32_t i = 1; i <= count; ++i) {
            if (options.camera_index != 0 && i != options.camera_index) continue;
            GX_DEVICE_INFO info{};
            if (!CheckGX(GXGetDeviceInfo(i, &info), "GXGetDeviceInfo", false)) continue;
            if (info.emDevType == GX_DEVICE_CLASS_U3V || info.emDevType == GX_DEVICE_CLASS_GEV) {
                camera_index = i;
                break;
            }
            RM_LOG_WARN("Skip camera index={}, unsupported type={}", i, info.emDevType);
        }
        if (camera_index == 0) throw std::runtime_error("No supported USB3/GigE camera found");
        CheckGX(GXOpenDeviceByIndex(camera_index, &device), "GXOpenDeviceByIndex");
        RM_LOG_INFO("Opened camera, SDK index={}", camera_index);

        // 3. 配置：关闭自动曝光/增益，连续采集，关闭触发。
        CheckGX(GXSetEnumValueByString(device, "ExposureAuto", "Off"), "ExposureAuto", false);
        CheckGX(GXSetEnumValueByString(device, "GainAuto", "Off"), "GainAuto", false);
        CheckGX(GXSetEnumValueByString(device, "BalanceWhiteAuto", "Continuous"), "BalanceWhiteAuto", false);
        CheckGX(GXSetEnumValueByString(device, "AcquisitionMode", "Continuous"), "AcquisitionMode");
        CheckGX(GXSetEnumValueByString(device, "TriggerMode", "Off"), "TriggerMode");
        if (!CheckGX(GXSetEnumValue(device, "PixelFormat", GX_PIXEL_FORMAT_BAYER_RG8), "BayerRG8", false))
            CheckGX(GXSetEnumValue(device, "PixelFormat", GX_PIXEL_FORMAT_MONO8), "Mono8");
        GX_INT_VALUE bandwidth{};
        if (CheckGX(GXGetIntValue(device, "DeviceLinkThroughputLimit", &bandwidth), "Get bandwidth", false)) {
            int64_t limit = bandwidth.nInc > 0
                ? bandwidth.nMin + (bandwidth.nMax - bandwidth.nMin) / bandwidth.nInc * bandwidth.nInc
                : bandwidth.nMax;
            CheckGX(GXSetIntValue(device, "DeviceLinkThroughputLimit", limit), "Set bandwidth", false);
        }
        CheckGX(GXSetAcqusitionBufferNumber(device, 5), "Set acquisition buffers");
        RM_LOG_DEBUG("Configuration complete: continuous acquisition, trigger off, 5 buffers");

        // 4. 开始取流。
        CheckGX(GXStreamOn(device), "GXStreamOn");
        streaming = true;
        RM_LOG_INFO("Streaming started; e/d exposure, a/q gain, s/w gamma, ESC exit");
        if (show_image) {
            cv::namedWindow("Daheng Camera", cv::WINDOW_NORMAL);
            window_ready = true;
        }
        int timeouts = 0, bad_frames = 0, fps_frames = 0;
        uint64_t total_frames = 0;
        auto start = std::chrono::steady_clock::now();
        double fps = 0;

        // 5. 取一帧 -> 转换图像 -> 归还缓存 -> 显示。
        while (!stop_requested) {
            GX_STATUS status = GXDQBuf(device, &frame, 1000);
            if (status == GX_STATUS_TIMEOUT) {
                CheckGX(status, "Frame timeout", false);
                if (++timeouts >= 3) throw std::runtime_error("Three consecutive frame timeouts");
                if (show_image && cv::waitKey(1) == 27) break;
                continue;
            }
            CheckGX(status, "GXDQBuf");
            timeouts = 0;
            if (!frame) throw std::runtime_error("SDK returned a null frame");
            if (frame->nStatus != GX_FRAME_STATUS_SUCCESS) {
                RM_LOG_WARN("Invalid frame id={}, status={}", frame->nFrameID, frame->nStatus);
                ReturnFrame(device, frame);
                if (++bad_frames >= 10) throw std::runtime_error("Ten consecutive invalid frames");
                if (show_image && cv::waitKey(1) == 27) break;
                continue;
            }
            bad_frames = 0;
            void* pixels = FrameData(*frame);
            if (!pixels || frame->nWidth <= 0 || frame->nHeight <= 0 || frame->nImgSize <= 0 ||
                static_cast<uint64_t>(frame->nWidth) * frame->nHeight > static_cast<uint64_t>(frame->nImgSize))
                throw std::runtime_error("Invalid frame size or image buffer");
            RM_LOG_TRACE("Frame id={}, size={}x{}, bytes={}",
                         frame->nFrameID, frame->nWidth, frame->nHeight, frame->nImgSize);
            cv::Mat image;
            if (frame->nPixelFormat == GX_PIXEL_FORMAT_MONO8) {
                image = cv::Mat(frame->nHeight, frame->nWidth, CV_8UC1, pixels).clone();
            } else if (frame->nPixelFormat == GX_PIXEL_FORMAT_BAYER_RG8) {
                image.create(frame->nHeight, frame->nWidth, CV_8UC3);
                VxInt32 dx_status = DxRaw8toRGB24Ex(pixels, image.data,
                    frame->nWidth, frame->nHeight, RAW2RGB_NEIGHBOUR, BAYERRG, false, DX_ORDER_BGR);
                if (dx_status != DX_OK) {
                    RM_LOG_ERROR("Image conversion failed: status={}", dx_status);
                    throw std::runtime_error("DxRaw8toRGB24Ex");
                }
            } else throw std::runtime_error("Unsupported frame pixel format");
            ReturnFrame(device, frame);

            ++total_frames;
            ++fps_frames;
            auto now = std::chrono::steady_clock::now();
            double seconds = std::chrono::duration<double>(now - start).count();
            if (seconds >= 1) {
                fps = fps_frames / seconds;
                RM_LOG_DEBUG("FPS={:.2f}, total frames={}", fps, total_frames);
                fps_frames = 0;
                start = now;
            }
            if (show_image) {
                cv::putText(image, "FPS: " + std::to_string(fps), cv::Point(10, 30),
                    cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar::all(255), 2);
                cv::imshow("Daheng Camera", image);
                int key = cv::waitKey(1);
                if (key == 27 || cv::getWindowProperty("Daheng Camera", cv::WND_PROP_VISIBLE) < 1) break;
                switch (key) {
                    case 'e': AdjustParameter(device, "ExposureTime", 25); break;
                    case 'd': AdjustParameter(device, "ExposureTime", -25); break;
                    case 'a': AdjustParameter(device, "Gain", 0.1); break;
                    case 'q': AdjustParameter(device, "Gain", -0.1); break;
                    case 's': AdjustParameter(device, "Gamma", 0.1); break;
                    case 'w': AdjustParameter(device, "Gamma", -0.1); break;
                    default: break;
                }
            }
            if (frame_limit > 0 && total_frames >= frame_limit) break;
        }
        RM_LOG_INFO("Acquisition finished, total frames={}", total_frames);
    } catch (const std::exception& e) {
        RM_LOG_ERROR("Program stopped: {}", e.what());
        result = 1;
    }

    // 6. 正常退出或发生异常，都要归还帧、停流、关相机、关 SDK。
    if (frame && !ReturnFrame(device, frame, false)) result = 1;
    if (streaming && !CheckGX(GXStreamOff(device), "GXStreamOff", false)) result = 1;
    if (device && !CheckGX(GXCloseDevice(device), "GXCloseDevice", false)) result = 1;
    if (library_ready && !CheckGX(GXCloseLib(), "GXCloseLib", false)) result = 1;
    if (window_ready) {
        try { cv::destroyAllWindows(); }
        catch (const std::exception& e) { RM_LOG_WARN("Close window failed: {}", e.what()); result = 1; }
    }
    RM_LOG_INFO("Program exit code={}", result);
    utils::RMLOG::instance().Close();
    return result;
}
