#pragma once

namespace sdk
{
	struct cengine_client
	{
		VIRTUAL(32, in_game(), bool(__thiscall*)(void*))();
		VIRTUAL(33, is_connected(), bool(__thiscall*)(void*))();
		VIRTUAL(50, get_screen_size(int &w, int &h), void(__thiscall *)(void *, int &, int &))(w, h);
	};

	struct cgame_ui_funcs
	{
		MFUNC(get_binding_for_button_code(int code), const char *(*)(void *, int), engine2, cgame_ui_funcs::get_binding_for_button_code)(code);
	};
} // namespace sdk
