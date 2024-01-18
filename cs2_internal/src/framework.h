#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <winnt.h>
#include <winternl.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include <tinyformat.h>

#pragma push_macro("max")
#pragma push_macro("min")
#undef max
#undef min

template <typename T>
__forceinline T clamp(const T &n, const T &lower, const T &upper)
{
	return std::max(lower, std::min(n, upper));
}

#pragma pop_macro("max")
#pragma pop_macro("min")

#include <sdk/generated.h>
#include <sdk/math/intrinsic.h>
#include <utils/defs.h>
