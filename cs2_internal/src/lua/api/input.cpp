#include "macros.h"
#include "gui/gui.h"
#include "lua/state.h"
#include "lua/api_def.h"
#include "lua/helpers.h"

int lua::api_def::input::is_key_down(lua_State *l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({{ltd::number}});

	if (!r)
	{
		s.error(XOR("usage: is_key_down(key)"));
		return 0;
	}

	s.push(evo::gui::input.is_key_down(s.get_integer(1)));
	return 1;
}

int lua::api_def::input::is_mouse_down(lua_State *l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({{ltd::number}});

	if (!r)
	{
		s.error(XOR("usage: is_mouse_down(mouse_key)"));
		return 0;
	}

	s.push(evo::gui::input.is_mouse_down(static_cast<char>(s.get_integer(1))));
	return 1;
}

int lua::api_def::input::get_cursor_pos(lua_State *l)
{
	runtime_state s(l);

	const auto pos = evo::gui::input.cursor();
	s.push(static_cast<int>(pos.x));
	s.push(static_cast<int>(pos.y));

	return 2;
}
