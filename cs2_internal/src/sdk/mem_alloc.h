#pragma once
#include <game/game.h>

namespace sdk
{
	struct cmem_alloc
	{
		VIRTUAL(1, alloc(size_t sz), void *(__thiscall *)(void *, size_t))(sz);
		VIRTUAL(2, realloc(void *p, size_t sz), void *(__thiscall *)(void *, void *, size_t))(p, sz);
		VIRTUAL(3, free(void *p), void(__thiscall *)(void *, void *))(p);
	};

	__forceinline char *mem_dup_str(const char *in)
	{
		return reinterpret_cast<char *(*)(const char *)>(game->tier0.at(offsets::functions::mem_alloc::str_dup_func))(in);
	}
} // namespace sdk
