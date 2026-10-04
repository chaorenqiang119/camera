// Simulated SDK with a deliberately simplified test contract, not vendor ABI.
// Never link this in production.
#include <cstddef>
#include <GxIAPI.h>
#include <DxImageProc.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

namespace {
bool library_ready = false, device_open = false, streaming = false, outstanding = false;
uint64_t frame_id = 0;
int64_t pixel_format = GX_PIXEL_FORMAT_BAYER_RG8;
unsigned char pixels[64 * 48]{};
GX_FRAME_BUFFER buffer{};
bool Scenario(const char* name) {
    const char* scenario = std::getenv("CAMERA_MOCK_SCENARIO");
    return scenario && std::string(scenario) == name;
}
void Event(const std::string& message) {
    if (const char* path = std::getenv("CAMERA_MOCK_EVENTS")) {
        std::ofstream output(path, std::ios::app);
        output << message << '\n';
    }
}
GX_STATUS Fail(const char* scenario) { return Scenario(scenario) ? GX_STATUS_ERROR : GX_STATUS_SUCCESS; }
}

GX_API GXInitLib() {
    Event("init");
    if (Scenario("init_failure")) return GX_STATUS_ERROR;
    library_ready = true;
    return GX_STATUS_SUCCESS;
}
GX_API GXCloseLib() {
    Event("close_lib");
    if (!library_ready || device_open) return GX_STATUS_INVALID_CALL;
    library_ready = false;
    return Fail("close_lib_failure");
}
GX_API GXGetLastError(GX_STATUS* error, char* text, size_t* size) {
    const char* message = "mock SDK injected failure";
    if (error) *error = GX_STATUS_ERROR;
    if (text) {
        if (*size < std::strlen(message) + 1) return GX_STATUS_ERROR;
        std::strcpy(text, message);
    }
    *size = std::strlen(message) + 1;
    return GX_STATUS_SUCCESS;
}
GX_API GXUpdateAllDeviceList(uint32_t* count, uint32_t) {
    Event("enumerate");
    *count = Scenario("no_device") ? 0 : Scenario("skip_first") ? 2 : 1;
    return Fail("enumerate_failure");
}
GX_API GXGetDeviceInfo(uint32_t index, GX_DEVICE_INFO* info) {
    Event("info " + std::to_string(index));
    *info = {};
    info->emDevType = Scenario("gige") ? GX_DEVICE_CLASS_GEV : GX_DEVICE_CLASS_U3V;
    if (Scenario("unsupported_device") || (Scenario("skip_first") && index == 1))
        info->emDevType = GX_DEVICE_CLASS_USB2;
    return Fail("info_failure");
}
GX_API GXOpenDeviceByIndex(uint32_t index, GX_DEV_HANDLE* device) {
    Event("open " + std::to_string(index));
    if (Scenario("open_failure")) return GX_STATUS_ERROR;
    device_open = true;
    *device = &buffer;
    return GX_STATUS_SUCCESS;
}
GX_API GXCloseDevice(GX_DEV_HANDLE) {
    Event("close_device");
    if (!device_open || streaming) return GX_STATUS_INVALID_CALL;
    device_open = false;
    return Fail("close_device_failure");
}
GX_API GXSetEnumValueByString(GX_PORT_HANDLE, const char* name, const char*) {
    Event(std::string("enum ") + name);
    const bool required = std::string(name) == "AcquisitionMode" || std::string(name) == "TriggerMode";
    if ((required && Scenario("config_failure")) || (!required && Scenario("optional_failure")))
        return GX_STATUS_ERROR;
    return GX_STATUS_SUCCESS;
}
GX_API GXSetEnumValue(GX_PORT_HANDLE, const char*, int64_t value) {
    Event("pixel_format");
    if (value == GX_PIXEL_FORMAT_BAYER_RG8 && Scenario("mono")) return GX_STATUS_ERROR;
    pixel_format = value;
    return GX_STATUS_SUCCESS;
}
GX_API GXGetIntValue(GX_PORT_HANDLE, const char*, GX_INT_VALUE* value) {
    *value = {};
    value->nMin = 1;
    value->nMax = 100;
    value->nInc = 3;
    return GX_STATUS_SUCCESS;
}
GX_API GXSetIntValue(GX_PORT_HANDLE, const char*, int64_t value) {
    Event("bandwidth " + std::to_string(value));
    return value == 100 ? GX_STATUS_SUCCESS : GX_STATUS_ERROR;
}
GX_API GXSetAcqusitionBufferNumber(GX_DEV_HANDLE, uint64_t count) {
    Event("buffers " + std::to_string(count));
    return count == 5 ? GX_STATUS_SUCCESS : GX_STATUS_ERROR;
}
GX_API GXGetFloatValue(GX_PORT_HANDLE, const char*, GX_FLOAT_VALUE* value) {
    *value = {};
    value->dCurValue = 50;
    value->dMin = 0;
    value->dMax = 100;
    return GX_STATUS_SUCCESS;
}
GX_API GXSetFloatValue(GX_PORT_HANDLE, const char*, double) { return GX_STATUS_SUCCESS; }
GX_API GXStreamOn(GX_DEV_HANDLE) {
    Event("stream_on");
    if (Scenario("stream_failure")) return GX_STATUS_ERROR;
    if (!device_open) return GX_STATUS_INVALID_CALL;
    streaming = true;
    return GX_STATUS_SUCCESS;
}
GX_API GXStreamOff(GX_DEV_HANDLE) {
    Event("stream_off");
    if (!streaming) return GX_STATUS_INVALID_CALL;
    streaming = false;
    outstanding = false; // SDK discards queued buffers on stopping acquisition.
    return Fail("stop_failure");
}
GX_API GXDQBuf(GX_DEV_HANDLE, PGX_FRAME_BUFFER* frame, uint32_t) {
    Event("dq");
    if (!streaming || outstanding) return GX_STATUS_INVALID_CALL;
    if (Scenario("timeout")) return GX_STATUS_TIMEOUT;
    if (Scenario("dq_failure")) return GX_STATUS_ERROR;
    if (Scenario("null_frame")) { *frame = nullptr; return GX_STATUS_SUCCESS; }
    buffer = {};
    buffer.nStatus = Scenario("bad_frames") ? static_cast<GX_FRAME_STATUS>(-1) : GX_FRAME_STATUS_SUCCESS;
#ifdef _WIN32
    buffer.pImgBuf = reinterpret_cast<uintptr_t>(pixels);
#else
    buffer.pImgBuf = pixels;
#endif
    buffer.nWidth = 64;
    buffer.nHeight = 48;
    buffer.nImgSize = sizeof(pixels);
    buffer.nFrameID = ++frame_id;
    buffer.nPixelFormat = static_cast<int32_t>(pixel_format);
    if (Scenario("short_frame")) buffer.nImgSize = 1;
    if (Scenario("invalid_dimensions")) buffer.nWidth = 0;
    if (Scenario("null_image")) buffer.pImgBuf = 0;
    if (Scenario("unsupported_format")) buffer.nPixelFormat = 0;
    outstanding = true;
    *frame = &buffer;
    return GX_STATUS_SUCCESS;
}
GX_API GXQBuf(GX_DEV_HANDLE, PGX_FRAME_BUFFER frame) {
    Event("q");
    if (!outstanding || frame != &buffer) return GX_STATUS_INVALID_CALL;
    outstanding = false;
    return Fail("q_failure");
}
VxInt32 DHDECL DxRaw8toRGB24Ex(void*, void* output, VxUint32 width, VxUint32 height,
    DX_BAYER_CONVERT_TYPE, DX_PIXEL_COLOR_FILTER, bool, DX_RGB_CHANNEL_ORDER) {
    Event("convert");
    if (Scenario("conversion_failure")) return -1;
    std::memset(output, 128, static_cast<size_t>(width) * height * 3);
    return DX_OK;
}
