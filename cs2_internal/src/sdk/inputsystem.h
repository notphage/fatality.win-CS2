#pragma once

#include <game/game.h>

namespace sdk
{
	struct cinput_system
	{
		MEMBER_CUSTOM(relative_mouse_mode, cinput_system, bool);
		MEMBER_CUSTOM(sdl_window, cinput_system, void*);

		MFUNC(vk_to_button_code(int vk), int(*)(void *, int), inputsystem, inputsystem::vk_to_button_code)(vk);
		MFUNC(set_cursor_pos(int x, int y), void (*)(void *, int, int, void *), inputsystem, inputsystem::set_cursor_pos)(x, y, nullptr);

		void set_input(const bool enabled)
		{
			if (!get_relative_mouse_mode())
				return;

			game->fn.sdl_set_relative_mouse_mode(enabled);
			game->fn.sdl_set_window_grab(get_sdl_window(), enabled);
		}
	};
}
