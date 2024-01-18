#include <game/game.h>
#include <game/hook_manager.h>
#include <gui/gui.h>
#include <lua/engine.h>
#include <menu/menu.h>
#include <sdk/engine.h>
#include <sdk/inputsystem.h>

namespace hooks::input_system
{
	LRESULT wnd_proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if (!evo::gui::ctx)
			return hook_manager.wnd_proc->call(wnd, msg, wparam, lparam);

		// handle debounce on focus loss
		if (wnd != GetActiveWindow())
			evo::gui::input.debounce();

		lua::api.callback(
			FNV1A("on_input"), [&](lua::state &s) -> int
			{
				s.push(static_cast<int>(msg));
				s.push(static_cast<int>(wparam));
				s.push(static_cast<int>(lparam));

				return 3;
			});

		const auto handle_gui_input = [&]()
		{
			if (!evo::gui::ctx->active && msg == XOR_32(WM_KEYDOWN))
			{
				if (wparam == XOR_32(VK_DELETE) || wparam == XOR_32(VK_INSERT) || wparam == XOR_32(VK_PAUSE))
				{
					menu::men.toggle();
					return true;
				}

				if (wparam == XOR_32(VK_ESCAPE) && menu::men.is_open())
				{
					if (!evo::gui::ctx->active_popups.empty())
						evo::gui::ctx->active_popups.back()->close();
					else
						menu::men.toggle();

					return true;
				}
			}

			const auto is_menu_open = menu::men.is_open();
			//const auto chat = (csgo_hud_chat_t *)interfaces::hud()->FindElement(XOR_STR("CCSGO_HudChat"));

			evo::gui::ctx->should_process_hotkeys = true; //!((interfaces::game_console()->IsConsoleVisible() || chat && chat->is_open) && !is_menu_open);
			return evo::gui::ctx->refresh(msg, wparam, lparam) && is_menu_open;
		};

		game->input_system->set_input(!menu::men.is_open());

		// handle menu movement
		auto input_enable = !handle_gui_input();
		if ((msg == XOR_32(WM_KEYDOWN) || msg == XOR_32(WM_KEYUP) || msg == XOR_32(WM_SYSKEYDOWN) || msg == XOR_32(WM_SYSKEYUP)) && menu::men.is_open())
		{
			if (evo::gui::ctx->active && evo::gui::ctx->active->is_taking_text)
				return hook_manager.wnd_proc->call(wnd, msg, wparam, lparam);

			const auto button_code = game->input_system->vk_to_button_code(wparam);
			if (const auto binding = game->game_ui_funcs->get_binding_for_button_code(button_code); binding)
			{
				switch (utils::fnv1a(binding))
				{
					case FNV1A("+forward"):
					case FNV1A("+back"):
					case FNV1A("+left"):
					case FNV1A("+right"):
					case FNV1A("+jump"):
					case FNV1A("+duck"):
					case FNV1A("+sprint"):
					case FNV1A("+lookatweapon"):
					case FNV1A("lastinv"):
					/*case FNV1A("invprev"):
					case FNV1A("invnext"):*/
						input_enable = true;
						break;
				}

				if (std::string(binding).starts_with(XOR("slot")))
					input_enable = true;
			}
		}

		return !input_enable || hook_manager.wnd_proc->call(wnd, msg, wparam, lparam);
	}

	bool mouse_input_enabled(void *rcx)
	{
		if (menu::men.is_open())
			return false;

		return hook_manager.mouse_input_enabled->call(rcx);
	}
}
