#pragma once
// Independently written test fixture for the operations used by this application.
// Layouts and values are simplified for testing and NOT compatible with Galaxy ABI.
// Real builds must use the official installed SDK headers and libraries.
#include <cstddef>
#include <cstdint>

using GX_STATUS = int;
using GX_FRAME_STATUS = int;
using GX_DEV_HANDLE = void*;
using GX_PORT_HANDLE = void*;
inline constexpr GX_STATUS GX_STATUS_SUCCESS = 0;
inline constexpr GX_STATUS GX_STATUS_ERROR = -1;
inline constexpr GX_STATUS GX_STATUS_TIMEOUT = -2;
inline constexpr GX_STATUS GX_STATUS_INVALID_CALL = -3;
inline constexpr GX_FRAME_STATUS GX_FRAME_STATUS_SUCCESS = 0;
inline constexpr int GX_DEVICE_CLASS_USB2 = 1;
inline constexpr int GX_DEVICE_CLASS_GEV = 2;
inline constexpr int GX_DEVICE_CLASS_U3V = 3;
inline constexpr int GX_PIXEL_FORMAT_MONO8 = 1;
inline constexpr int GX_PIXEL_FORMAT_BAYER_RG8 = 2;

struct GX_DEVICE_INFO { int emDevType = 0; };
struct GX_INT_VALUE { int64_t nCurValue = 0, nMin = 0, nMax = 0, nInc = 0; };
struct GX_FLOAT_VALUE {
    double dCurValue = 0, dMin = 0, dMax = 0, dInc = 0;
    bool bIncIsValid = false;
};
struct GX_FRAME_BUFFER {
    GX_FRAME_STATUS nStatus = 0;
#ifdef _WIN32
    uintptr_t pImgBuf = 0;
#else
    void* pImgBuf = nullptr;
#endif
    int32_t nWidth = 0, nHeight = 0, nPixelFormat = 0, nImgSize = 0;
    uint64_t nFrameID = 0;
};
using PGX_FRAME_BUFFER = GX_FRAME_BUFFER*;
#define GX_API extern "C" GX_STATUS

GX_API GXInitLib();
GX_API GXCloseLib();
GX_API GXGetLastError(GX_STATUS*, char*, size_t*);
GX_API GXUpdateAllDeviceList(uint32_t*, uint32_t);
GX_API GXGetDeviceInfo(uint32_t, GX_DEVICE_INFO*);
GX_API GXOpenDeviceByIndex(uint32_t, GX_DEV_HANDLE*);
GX_API GXCloseDevice(GX_DEV_HANDLE);
GX_API GXSetEnumValueByString(GX_PORT_HANDLE, const char*, const char*);
GX_API GXSetEnumValue(GX_PORT_HANDLE, const char*, int64_t);
GX_API GXGetIntValue(GX_PORT_HANDLE, const char*, GX_INT_VALUE*);
GX_API GXSetIntValue(GX_PORT_HANDLE, const char*, int64_t);
GX_API GXSetAcqusitionBufferNumber(GX_DEV_HANDLE, uint64_t);
GX_API GXGetFloatValue(GX_PORT_HANDLE, const char*, GX_FLOAT_VALUE*);
GX_API GXSetFloatValue(GX_PORT_HANDLE, const char*, double);
GX_API GXStreamOn(GX_DEV_HANDLE);
GX_API GXStreamOff(GX_DEV_HANDLE);
GX_API GXDQBuf(GX_DEV_HANDLE, PGX_FRAME_BUFFER*, uint32_t);
GX_API GXQBuf(GX_DEV_HANDLE, PGX_FRAME_BUFFER);
