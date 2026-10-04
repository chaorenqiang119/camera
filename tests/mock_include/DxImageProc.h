#pragma once
// Test-only conversion contract. No vendor header, algorithm or ABI is provided.
#include <cstdint>
using VxInt32 = int32_t;
using VxUint32 = uint32_t;
using DX_BAYER_CONVERT_TYPE = int;
using DX_PIXEL_COLOR_FILTER = int;
using DX_RGB_CHANNEL_ORDER = int;
inline constexpr int RAW2RGB_NEIGHBOUR = 0;
inline constexpr int BAYERRG = 0;
inline constexpr int DX_ORDER_BGR = 0;
inline constexpr int DX_OK = 0;
#define DHDECL
extern "C" VxInt32 DHDECL DxRaw8toRGB24Ex(void*, void*, VxUint32, VxUint32,
    DX_BAYER_CONVERT_TYPE, DX_PIXEL_COLOR_FILTER, bool, DX_RGB_CHANNEL_ORDER);
