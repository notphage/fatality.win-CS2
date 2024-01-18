#include "api_def.h"
#include "engine.h"
#include "engine_net.h"
#include "helpers.h"
#include <gui/gui.h>
#include <game/cfg.h>

using namespace evo::ren;

__declspec(noinline) bool lua::script::initialize()
{
	// set panic function
	l.set_panic_func(api_def::panic);

	// hide poor globals first
	hide_globals();

	// register global functions
	register_globals();

	// register namespaces
	register_namespaces();

	// register types
	register_types();

	// attempt loading file
#ifdef CS2_CLOUD
	VIRTUALIZER_TIGER_WHITE_START;

	if (remote.is_proprietary)
	{
		if (net_data.enc_key.empty())
			return false;

		std::ifstream f(file, std::ios::binary);
		if (!f.is_open())
			return {};

		std::string content;
		f.seekg(0, std::ios::end);
		content.reserve(f.tellg());
		f.seekg(0, std::ios::beg);
		content.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
		f.close();

		if (!l.load_string(util::aes256_decrypt(content, net_data.enc_key)))
		{
			lua::helpers::error(XOR("syntax_error"), l.get_last_error_description().c_str());
			return false;
		}

		return true;
	}

	VIRTUALIZER_TIGER_WHITE_END;
#endif

	if (!l.load_file(file.c_str()))
	{
		lua::helpers::error(XOR("syntax_error"), l.get_last_error_description().c_str());
		return false;
	}

	return true;
}

__attribute__( ( always_inline ) ) void lua::script::unload()
{
	using namespace evo::gui;

	// resolve parent and erase element, freeing it
	for (const auto &e : gui_elements)
	{
		// remove from parent
		if (const auto &c = ctx->find(e))
		{
			if (const auto p = c->parent.lock(); p && p->is_container)
				p->as<control_container>()->remove(e);
			else
				ctx->remove(e);
		}

		// remove from hotkey list
		if (ctx->hotkey_list.find(e) != ctx->hotkey_list.end())
			ctx->hotkey_list.erase(e);
	}

	// erase callbacks
	for (const auto &e : gui_callbacks)
	{
		if (const auto &c = ctx->find(e))
		{
			if (c->universal_callbacks.find(id) != c->universal_callbacks.end())
				c->universal_callbacks[id].clear();
		}
	}

	for (const auto &f : fonts)
	{
		if (const auto &e = draw.fonts[f]; e)
		{
			e->destroy();
			draw.fonts[f] = nullptr;
		}

		api.font.on_free(f);
	}

	for (const auto &t : textures)
	{
		if (const auto &e = draw.textures[t]; e)
		{
			e->destroy();
			draw.textures[t] = nullptr;
		}

		api.texture.on_free(t);
	}

	for (const auto &t : shaders)
	{
		if (const auto &e = draw.shaders[t]; e)
		{
			e->destroy();
			draw.shaders[t] = nullptr;
		}

		api.shader.on_free(t);
	}

	for (const auto &a : animators)
	{
		if (api.anims[a])
			api.anims[a] = nullptr;

		api.animator.on_free(a);
	}

	if (helpers::timers.contains(id))
		helpers::timers[id].clear();

	fonts.clear();
	textures.clear();
	gui_elements.clear();
	net_ids.clear();
}

__attribute__( ( always_inline ) ) bool lua::script::call_main()
{
	// taking off!
	if (!l.run())
	{
		lua::helpers::error(XOR("runtime_error"), l.get_last_error_description().c_str());
		return false;
	}

	// find all forwards
	find_forwards();
	return true;
}

void lua::script::find_forwards()
{
	// declare list of all possible forwards
	const std::vector<std::string> forward_list = {XOR("on_paint"), XOR("on_paint_traverse"), XOR("on_frame_stage_notify"), XOR("on_setup_move"), XOR("on_run_command"),
												   XOR("on_create_move"), XOR("on_level_init"), XOR("on_do_post_screen_space_events"), XOR("on_input"),
												   XOR("on_game_event"), XOR("on_shutdown"), XOR("on_config_load"), XOR("on_config_save"), XOR("on_shot_registered"),
												   XOR("on_console_input"), XOR("on_esp_flags"), XOR("on_draw_model_execute"),};

	// iterate through the list and check if we find any functions
	for (const auto &f : forward_list)
	{
		l.get_global(f.c_str());
		if (l.is_function(-1))
			forwards[utils::fnv1a(f.c_str())] = f;

		l.pop(1);
	}
}

void lua::script::create_forward(const char *n)
{
	forwards[utils::fnv1a(n)] = n;
}

bool lua::script::has_forward(uint32_t hash)
{
	return forwards.find(hash) != forwards.end();
}

void lua::script::call_forward(uint32_t hash, const std::function<int(state &)> &arg_callback, int ret, const std::function<void(state &)> &callback)
{
	// check if forward exists
	if (!is_running || !has_forward(hash))
		return;

	// call the function
	l.get_global(forwards[hash].c_str());
	if (l.is_nil(-1))
	{
		l.pop(1);
		return;
	}

	if (!l.call(arg_callback ? arg_callback(l) : 0, ret))
	{
		did_error = true;
		lua::helpers::error(XOR("runtime_error"), l.get_last_error_description().c_str());
		if (const auto f = api.find_script_file(id); f.has_value())
			f->get().should_unload = true;

		return;
	}

	if (ret)
	{
		if (l.get_stack_top() < ret)
		{
			did_error = true;
			lua::helpers::error(XOR("runtime_error"), XOR("Not enough return values."));
			if (const auto f = api.find_script_file(id); f.has_value())
				f->get().should_unload = true;

			return;
		}

		if (callback)
			callback(l);

		l.pop(l.get_stack_top());
	}
}

void lua::script::hide_globals()
{
	l.unset_global(XOR("getfenv")); // allows getting environment table
	if (!cfg.lua.allow_insecure)
	{
		l.unset_global(XOR("pcall")); // allows raw calling
		l.unset_global(XOR("xpcall")); // ^
	}

	l.unset_global(XOR("loadfile"));
	if (!cfg.lua.allow_insecure)
	{
		// allows direct file load
		l.unset_global(XOR("load")); // allows direct load
		l.unset_global(XOR("loadstring")); // allows direct string load
		l.unset_global(XOR("dofile")); // same as loadfile but also executes it
	}

	l.unset_global(XOR("gcinfo")); // garbage collector info
	l.unset_global(XOR("collectgarbage")); // forces GC to collect
	l.unset_global(XOR("newproxy")); // proxy functions
	l.unset_global(XOR("coroutine")); // allows managing coroutines
	l.unset_global(XOR("setfenv")); // allows replacing environment table

	if (!cfg.lua.allow_insecure)
	{
		l.unset_global(XOR("rawget")); // bypasses metatables
		l.unset_global(XOR("rawset")); // ^
		l.unset_global(XOR("rawequal")); // ^
	}

	l.unset_in_table(XOR("ffi"), XOR("C")); // useless without load()
	l.unset_global(XOR("_G")); // disable global table loop
	l.unset_in_table(XOR("ffi"), XOR("load")); // allows loading DLLs
	l.unset_in_table(XOR("ffi"), XOR("gc")); // forces GC to collect
	l.unset_in_table(XOR("ffi"), XOR("fill")); // memory setting

	l.unset_in_table(XOR("string"), XOR("dump")); // useless without load()
}

void lua::script::register_namespaces()
{
	l.create_namespace(
		XOR("render"), {{XOR("color"), api_def::render::color}, {XOR("rect_filled"), api_def::render::rect_filled}, {XOR("rect"), api_def::render::rect},
						{XOR("rect_filled_rounded"), api_def::render::rect_filled_rounded}, {XOR("text"), api_def::render::text},
						{XOR("get_screen_size"), api_def::render::get_screen_size}, {XOR("create_font"), api_def::render::create_font},
						{XOR("create_font_gdi"), api_def::render::create_font_gdi}, {XOR("get_text_size"), api_def::render::get_text_size},
						{XOR("circle"), api_def::render::circle}, {XOR("circle_filled"), api_def::render::circle_filled},
						{XOR("create_texture"), api_def::render::create_texture}, {XOR("create_texture_bytes"), api_def::render::create_texture_bytes},
						{XOR("create_texture_rgba"), api_def::render::create_texture_rgba}, {XOR("set_texture"), api_def::render::set_texture},
						{XOR("push_texture"), api_def::render::set_texture}, {XOR("pop_texture"), api_def::render::pop_texture},
						{XOR("get_texture_size"), api_def::render::get_texture_size}, {XOR("create_animator_float"), api_def::render::create_animator_float},
						{XOR("create_animator_color"), api_def::render::create_animator_color}, {XOR("push_clip_rect"), api_def::render::push_clip_rect},
						{XOR("pop_clip_rect"), api_def::render::pop_clip_rect}, {XOR("set_uv"), api_def::render::set_uv}, {XOR("push_uv"), api_def::render::set_uv},
						{XOR("pop_uv"), api_def::render::pop_uv}, {XOR("rect_filled_multicolor"), api_def::render::rect_filled_multicolor},
						{XOR("line"), api_def::render::line}, {XOR("line_multicolor"), api_def::render::line_multicolor}, {XOR("triangle"), api_def::render::triangle},
						{XOR("triangle_filled"), api_def::render::triangle_filled}, {XOR("triangle_filled_multicolor"), api_def::render::triangle_filled_multicolor},
						//{XOR("create_texture_svg"), api_def::render::create_texture_svg},
						//{XOR("create_texture_stream"), api_def::render::create_texture_stream},
						{XOR("create_font_stream"), api_def::render::create_font_stream}, {XOR("set_alpha"), api_def::render::set_alpha}, // not documented yet
						{XOR("get_alpha"), api_def::render::get_alpha}, // not documented yet
						{XOR("push_alpha"), api_def::render::set_alpha}, // not documented yet
						{XOR("pop_alpha"), api_def::render::pop_alpha}, // not documented yet
						{XOR("blur"), api_def::render::blur}, {XOR("create_shader"), api_def::render::create_shader}, {XOR("set_shader"), api_def::render::set_shader},
						{XOR("wrap_text"), api_def::render::wrap_text}, {XOR("set_rotation"), api_def::render::set_rotation}, // not documented yet
						{XOR("set_frame"), api_def::render::set_frame}, // not documented yet
						{XOR("set_loop"), api_def::render::set_loop}, // not documented yet
						{XOR("reset_loop"), api_def::render::reset_loop}, // not documented yet
						{XOR("get_frame_count"), api_def::render::get_frame_count}, {XOR("rect_rounded"), api_def::render::rect_rounded},
						{XOR("circle_filled_multicolor"), api_def::render::circle_filled_multicolor},
						// not documented yet
						{XOR("set_anti_aliasing"), api_def::render::set_anti_aliasing}, // not documented yet
						{XOR("set_render_mode"), api_def::render::set_render_mode}, // not documented yet
		});

	l.set_integers_for_table(
		XOR("render"), {
			// alignment
			{XOR("align_left"), 0}, {XOR("align_top"), 0}, {XOR("align_center"), 1}, {XOR("align_right"), 2}, {XOR("align_bottom"), 2}, {XOR("top_left"), 1},
			{XOR("top_right"), 2}, {XOR("bottom_left"), 4}, {XOR("bottom_right"), 8}, {XOR("top"), 1 | 2}, {XOR("bottom"), 4 | 8}, {XOR("left"), 1 | 4},
			{XOR("right"), 2 | 8}, {XOR("all"), 1 | 2 | 4 | 8},

			// font flags
			{XOR("font_flag_shadow"), 1}, {XOR("font_flag_outline"), 2}, {XOR("font_flag_anti_alias"), 4},

			// easing
			{XOR("linear"), 0}, {XOR("ease_in"), 1}, {XOR("ease_out"), 2}, {XOR("ease_in_out"), 3},

			// render modes
			{XOR("mode_normal"), 0}, {XOR("mode_wireframe"), 1},

			// outline modes
			{XOR("outline_inset"), 0}, {XOR("outline_outset"), 1}, {XOR("outline_center"), 2},

			// esp sides
			{XOR("esp_left"), 0}, {XOR("esp_right"), 1}, {XOR("esp_top"), 2}, {XOR("esp_bottom"), 3}});

	l.set_uint64s_for_table(
		XOR("render"), {{XOR("font_gui_main"), FNV1A("gui_main")}, {XOR("font_gui_title"), FNV1A("gui_title")}, {XOR("font_gui_bold"), GUI_HASH("gui_bold")},
						{XOR("font_esp"), GUI_HASH("esp")}, {XOR("font_esp_name"), GUI_HASH("esp_name")}, {XOR("font_indicator"), GUI_HASH("lby")},
						{XOR("texture_logo_head"), GUI_HASH("gui_logo_head")}, {XOR("texture_logo_stripes"), GUI_HASH("gui_logo_stripes")},
						{XOR("texture_cursor"), GUI_HASH("gui_cursor")}, {XOR("texture_loading"), GUI_HASH("gui_loading")},
						{XOR("texture_icon_up"), GUI_HASH("gui_icon_up")}, {XOR("texture_icon_down"), GUI_HASH("gui_icon_down")},
						{XOR("texture_icon_clear"), GUI_HASH("gui_icon_clear")}, {XOR("texture_icon_copy"), GUI_HASH("gui_icon_copy")},
						{XOR("texture_icon_paste"), GUI_HASH("gui_icon_paste")}, {XOR("texture_icon_add"), GUI_HASH("gui_icon_add")},
						{XOR("texture_icon_search"), GUI_HASH("gui_icon_search")}, {XOR("texture_icon_settings"), GUI_HASH("gui_icon_settings")},
						{XOR("texture_icon_bug"), GUI_HASH("gui_icon_bug")}, {XOR("texture_icon_rage"), GUI_HASH("icon_rage")},
						{XOR("texture_icon_legit"), GUI_HASH("icon_legit")}, {XOR("texture_icon_visuals"), GUI_HASH("icon_visuals")},
						{XOR("texture_icon_misc"), GUI_HASH("icon_misc")}, {XOR("texture_icon_scripts"), GUI_HASH("icon_scripts")},
						{XOR("texture_icon_skins"), GUI_HASH("icon_skins")}, {XOR("texture_avatar"), GUI_HASH("gui_user_avatar")},
						{XOR("back_buffer"), GUI_HASH("back_buffer")}, {XOR("back_buffer_downsampled"), GUI_HASH("back_buffer_downsampled")},});

	l.create_namespace(
		XOR("utils"), {
			//{XOR("random_int"), api_def::utils::random_int},
			//{XOR("random_float"), api_def::utils::random_float},
			{XOR("flags"), api_def::utils::flags}, {XOR("find_interface"), api_def::utils::find_interface}, {XOR("find_pattern"), api_def::utils::find_pattern},
			//{XOR("get_weapon_info"), api_def::utils::get_weapon_info},
			{XOR("new_timer"), api_def::utils::new_timer}, {XOR("run_delayed"), api_def::utils::run_delayed},
			//{XOR("world_to_screen"), api_def::utils::world_to_screen},
			//{XOR("get_rtt"), api_def::utils::get_rtt},
			{XOR("get_time"), api_def::utils::get_time}, {XOR("http_get"), api_def::utils::http_get}, {XOR("http_post"), api_def::utils::http_post},
			{XOR("json_encode"), api_def::utils::json_encode}, {XOR("json_decode"), api_def::utils::json_decode},
			//{XOR("set_clan_tag"), api_def::utils::set_clan_tag},
			//{XOR("trace"), api_def::utils::trace},
			//{XOR("trace_bullet"), api_def::utils::trace_bullet},
			//{XOR("scale_damage"), api_def::utils::scale_damage},
			{XOR("load_file"), api_def::utils::load_file}, {XOR("print_console"), api_def::utils::print_console},
			//{XOR("print_dev_console"), api_def::utils::print_dev_console},
			{XOR("error_print"), api_def::utils::error_print},
			//{XOR("aes256_encrypt"), api_def::utils::aes256_encrypt},
			//{XOR("aes256_decrypt"), api_def::utils::aes256_decrypt},
			{XOR("base64_encode"), api_def::utils::base64_encode}, {XOR("base64_decode"), api_def::utils::base64_decode},
			{XOR("get_unix_time"), api_def::utils::get_unix_time},});

	l.create_namespace(XOR("client"), {{XOR("create_interface"), api_def::utils::find_interface}, {XOR("find_signature"), api_def::utils::find_pattern},});

	l.create_namespace(
		XOR("gui"), {{XOR("textbox"), api_def::gui::textbox_construct}, {XOR("checkbox"), api_def::gui::checkbox_construct},
					 {XOR("slider"), api_def::gui::slider_construct}, {XOR("get_checkbox"), api_def::gui::get_checkbox}, {XOR("get_slider"), api_def::gui::get_slider},
					 {XOR("add_notification"), api_def::gui::add_notification}, {XOR("for_each_hotkey"), api_def::gui::for_each_hotkey},
					 {XOR("is_menu_open"), api_def::gui::is_menu_open}, {XOR("get_combobox"), api_def::gui::get_combobox},
					 {XOR("combobox"), api_def::gui::combobox_construct}, {XOR("label"), api_def::gui::label_construct}, {XOR("button"), api_def::gui::button_construct},
					 {XOR("color_picker"), api_def::gui::color_picker_construct}, {XOR("get_color_picker"), api_def::gui::get_color_picker},
					 {XOR("show_message"), api_def::gui::show_message}, {XOR("show_dialog"), api_def::gui::show_dialog},
					 {XOR("get_menu_rect"), api_def::gui::get_menu_rect}, {XOR("list"), api_def::gui::list_construct},});

	l.set_integers_for_table(
		XOR("gui"), {{XOR("hotkey_toggle"), 0}, {XOR("hotkey_hold"), 1}, {XOR("dialog_buttons_ok_cancel"), 0}, {XOR("dialog_buttons_yes_no"), 1},
					 {XOR("dialog_buttons_yes_no_cancel"), 2}, {XOR("dialog_result_affirmative"), 0}, {XOR("dialog_result_negative"), 1},});

	l.create_namespace(
		XOR("input"), {{XOR("is_key_down"), api_def::input::is_key_down}, {XOR("is_mouse_down"), api_def::input::is_mouse_down},
					   {XOR("get_cursor_pos"), api_def::input::get_cursor_pos},});

	/*l.extend_namespace(XOR("math"), {
						   {XOR("vec3"), api_def::math::vec3_new},
						   {XOR("vector_angles"), api_def::math::vector_angles},
						   {XOR("angle_vectors"), api_def::math::angle_vectors}
					   });*/
}

void lua::script::register_globals()
{
	l.set_global_function(XOR("print"), api_def::print);
	l.set_global_function(XOR("require"), api_def::require);
	l.set_global_function(XOR("loadfile"), api_def::loadfile);

	l.create_table();
	{
		l.set_field(XOR("save"), api_def::database::save);
		l.set_field(XOR("load"), api_def::database::load);

		l.create_metatable(XOR("database"));
		l.set_field(XOR("__newindex"), api_def::stub_new_index);
		l.set_field(XOR("__index"), api_def::stub_index);
		l.set_metatable();

	}
	l.set_global(XOR("database"));

	/*l.create_table();
	{
		l.set_field(XOR("is_in_game"), api_def::engine::is_in_game);
		l.set_field(XOR("exec"), api_def::engine::exec);
		l.set_field(XOR("get_local_player"), api_def::engine::get_local_player);
		l.set_field(XOR("get_view_angles"), api_def::engine::get_view_angles);
		l.set_field(XOR("get_player_for_user_id"), api_def::engine::get_player_for_user_id);
		l.set_field(XOR("get_player_info"), api_def::engine::get_player_info);
		l.set_field(XOR("set_view_angles"), api_def::engine::set_view_angles);

		l.create_metatable(XOR("engine"));
		l.set_field(XOR("__newindex"), api_def::stub_new_index);
		l.set_field(XOR("__index"), api_def::stub_index);
		l.set_metatable();

	}
	l.set_global(XOR("engine"));*/

	/*l.create_table();
	{
		l.set_field(XOR("get_entity"), api_def::entities::get_entity);
		l.set_field(XOR("get_entity_from_handle"), api_def::entities::get_entity_from_handle);
		l.set_field(XOR("for_each"), api_def::entities::for_each);
		l.set_field(XOR("for_each_z"), api_def::entities::for_each_z);
		l.set_field(XOR("for_each_player"), api_def::entities::for_each_player);
		l.set_field(XOR("for_each_player_z"), api_def::entities::for_each_player_z);

		l.create_metatable(XOR("entities"));
		l.set_field(XOR("__newindex"), api_def::stub_new_index);
		l.set_field(XOR("__index"), api_def::entities::index);
		l.set_metatable();

	}
	l.set_global(XOR("entities"));*/

	/*l.create_table();
	{
		l.create_table();
		{
			l.create_metatable(XOR("fatality"));
			l.set_field(XOR("__newindex"), api_def::stub_new_index);
			l.set_field(XOR("__index"), api_def::fatality_index);
			l.set_metatable();
		}
		l.set_field(XOR("fatality"));

		l.create_table();
		{
			l.create_metatable(XOR("server"));
			l.set_field(XOR("__newindex"), api_def::stub_new_index);
			l.set_field(XOR("__index"), api_def::server_index);
			l.set_metatable();
		}
		l.set_field(XOR("server"));
	}
	l.set_global(XOR("info"));*/

	/*l.create_table();
	{
		l.create_metatable(XOR("cvar"));
		l.set_field(XOR("__newindex"), api_def::stub_new_index);
		l.set_field(XOR("__index"), api_def::cvar::index);
		l.set_metatable();
	}
	l.set_global(XOR("cvar"));*/

	l.create_table();
	{
		l.create_metatable(XOR("global_vars"));
		l.set_field(XOR("__newindex"), api_def::stub_new_index);
		l.set_field(XOR("__index"), api_def::global_vars_index);
		l.set_metatable();
	}
	l.set_global(XOR("global_vars"));

	/*l.create_table();
	{
		l.create_metatable(XOR("game_rules"));
		l.set_field(XOR("__newindex"), api_def::stub_new_index);
		l.set_field(XOR("__index"), api_def::game_rules_index);
		l.set_metatable();
	}
	l.set_global(XOR("game_rules"));*/

	/*l.create_namespace(XOR("mat"), {
						   {XOR("create"), api_def::mat::create},
						   {XOR("find"), api_def::mat::find},
						   {XOR("for_each_material"), api_def::mat::for_each_material},
						   {XOR("override_material"), api_def::mat::override_material},
					   });*/

	/*l.set_integers_for_table(XOR("mat"), {
								 {XOR("var_debug"), MATERIAL_VAR_DEBUG},
								 {XOR("var_no_debug_override"), MATERIAL_VAR_NO_DEBUG_OVERRIDE},
								 {XOR("var_no_draw"), MATERIAL_VAR_NO_DRAW},
								 {XOR("var_use_in_fillrate_mode"), MATERIAL_VAR_USE_IN_FILLRATE_MODE},
								 {XOR("var_vertexcolor"), MATERIAL_VAR_VERTEXCOLOR},
								 {XOR("var_vertexalpha"), MATERIAL_VAR_VERTEXALPHA},
								 {XOR("var_selfillum"), MATERIAL_VAR_SELFILLUM},
								 {XOR("var_additive"), MATERIAL_VAR_ADDITIVE},
								 {XOR("var_alphatest"), MATERIAL_VAR_ALPHATEST},
								 {XOR("var_znearer"), MATERIAL_VAR_ZNEARER},
								 {XOR("var_model"), MATERIAL_VAR_MODEL},
								 {XOR("var_flat"), MATERIAL_VAR_FLAT},
								 {XOR("var_nocull"), MATERIAL_VAR_NOCULL},
								 {XOR("var_nofog"), MATERIAL_VAR_NOFOG},
								 {XOR("var_ignorez"), MATERIAL_VAR_IGNOREZ},
								 {XOR("var_decal"), MATERIAL_VAR_DECAL},
								 {XOR("var_envmapsphere"), MATERIAL_VAR_ENVMAPSPHERE},
								 {XOR("var_envmapcameraspace"), MATERIAL_VAR_ENVMAPCAMERASPACE},
								 {XOR("var_basealphaenvmapmask"), MATERIAL_VAR_BASEALPHAENVMAPMASK},
								 {XOR("var_translucent"), MATERIAL_VAR_TRANSLUCENT},
								 {XOR("var_normalmapalphaenvmapmask"), MATERIAL_VAR_NORMALMAPALPHAENVMAPMASK},
								 {XOR("var_needs_software_skinning"), MATERIAL_VAR_NEEDS_SOFTWARE_SKINNING},
								 {XOR("var_opaquetexture"), MATERIAL_VAR_OPAQUETEXTURE},
								 {XOR("var_envmapmode"), MATERIAL_VAR_ENVMAPMODE},
								 {XOR("var_suppress_decals"), MATERIAL_VAR_SUPPRESS_DECALS},
								 {XOR("var_halflambert"), MATERIAL_VAR_HALFLAMBERT},
								 {XOR("var_wireframe"), MATERIAL_VAR_WIREFRAME},
								 {XOR("var_allowalphatocoverage"), MATERIAL_VAR_ALLOWALPHATOCOVERAGE},
								 {XOR("var_alpha_modified_by_proxy"), MATERIAL_VAR_ALPHA_MODIFIED_BY_PROXY},
								 {XOR("var_vertexfog"), MATERIAL_VAR_VERTEXFOG},
							 });*/

	/*l.create_table();
	{
		l.set_field(XOR("frame_undefined"), -1);
		l.set_field(XOR("frame_start"), 0);
		l.set_field(XOR("frame_net_update_start"), 1);
		l.set_field(XOR("frame_net_update_postdataupdate_start"), 2);
		l.set_field(XOR("frame_net_update_postdataupdate_end"), 3);
		l.set_field(XOR("frame_net_update_end"), 4);
		l.set_field(XOR("frame_render_start"), 5);
		l.set_field(XOR("frame_render_end"), 6);
		l.set_field(XOR("in_attack"), IN_ATTACK);
		l.set_field(XOR("in_jump"), IN_JUMP);
		l.set_field(XOR("in_duck"), IN_DUCK);
		l.set_field(XOR("sdk::schema::in_forward"), sdk::schema::in_forward);
		l.set_field(XOR("in_back"), IN_BACK);
		l.set_field(XOR("in_use"), IN_USE);
		l.set_field(XOR("in_left"), IN_LEFT);
		l.set_field(XOR("in_move_left"), IN_MOVELEFT);
		l.set_field(XOR("in_right"), IN_RIGHT);
		l.set_field(XOR("in_move_right"), IN_MOVERIGHT);
		l.set_field(XOR("in_attack2"), IN_ATTACK2);
		l.set_field(XOR("in_score"), IN_SCORE);
	}
	l.set_global(XOR("csgo"));*/
}

void lua::script::register_types()
{
	l.create_type(
		XOR("gui.textbox"), {{XOR("get_value"), api_def::gui::textbox_get_value}, {XOR("get"), api_def::gui::textbox_get_value},
							 {XOR("set_value"), api_def::gui::textbox_set_value}, {XOR("set"), api_def::gui::textbox_set_value},
							 {XOR("set_tooltip"), api_def::gui::control_set_tooltip}, {XOR("set_visible"), api_def::gui::control_set_visible},
							 {XOR("add_callback"), api_def::gui::control_add_callback}, {XOR("get_name"), api_def::gui::control_get_name},
							 {XOR("get_type"), api_def::gui::control_get_type},});

	l.create_type(
		XOR("gui.checkbox"), {{XOR("get_value"), api_def::gui::checkbox_get_value}, {XOR("get"), api_def::gui::checkbox_get_value},
							  {XOR("set_value"), api_def::gui::checkbox_set_value}, {XOR("set"), api_def::gui::checkbox_set_value},
							  {XOR("set_tooltip"), api_def::gui::control_set_tooltip}, {XOR("set_visible"), api_def::gui::control_set_visible},
							  {XOR("add_callback"), api_def::gui::control_add_callback}, {XOR("get_name"), api_def::gui::control_get_name},
							  {XOR("get_type"), api_def::gui::control_get_type},});

	l.create_type(
		XOR("gui.slider"), {{XOR("get_value"), api_def::gui::slider_get_value}, {XOR("get"), api_def::gui::slider_get_value},
							{XOR("set_value"), api_def::gui::slider_set_value}, {XOR("set"), api_def::gui::slider_set_value},
							{XOR("set_tooltip"), api_def::gui::control_set_tooltip}, {XOR("set_visible"), api_def::gui::control_set_visible},
							{XOR("add_callback"), api_def::gui::control_add_callback}, {XOR("get_name"), api_def::gui::control_get_name},
							{XOR("get_type"), api_def::gui::control_get_type},});

	l.create_type(
		XOR("gui.combobox"), {{XOR("get_value"), api_def::gui::combobox_get_value}, {XOR("get"), api_def::gui::combobox_get_value},
							  {XOR("set_value"), api_def::gui::combobox_set_value}, {XOR("set"), api_def::gui::combobox_set_value},
							  {XOR("set_tooltip"), api_def::gui::control_set_tooltip}, {XOR("set_visible"), api_def::gui::control_set_visible},
							  {XOR("add_callback"), api_def::gui::control_add_callback}, {XOR("get_options"), api_def::gui::combobox_get_options},
							  {XOR("get_name"), api_def::gui::control_get_name}, {XOR("get_type"), api_def::gui::control_get_type},});

	l.create_type(
		XOR("gui.color_picker"), {{XOR("get_value"), api_def::gui::color_picker_get_value}, {XOR("get"), api_def::gui::color_picker_get_value},
								  {XOR("set_value"), api_def::gui::color_picker_set_value}, {XOR("set"), api_def::gui::color_picker_set_value},
								  {XOR("set_tooltip"), api_def::gui::control_set_tooltip}, {XOR("set_visible"), api_def::gui::control_set_visible},
								  {XOR("add_callback"), api_def::gui::control_add_callback}, {XOR("get_name"), api_def::gui::control_get_name},
								  {XOR("get_type"), api_def::gui::control_get_type},});

	l.create_type(
		XOR("gui.label"), {{XOR("set_tooltip"), api_def::gui::control_set_tooltip}, {XOR("set_visible"), api_def::gui::control_set_visible},
						   {XOR("add_callback"), api_def::gui::control_add_callback}, {XOR("get_name"), api_def::gui::control_get_name},
						   {XOR("get_type"), api_def::gui::control_get_type},});

	l.create_type(
		XOR("gui.button"), {{XOR("set_tooltip"), api_def::gui::control_set_tooltip}, {XOR("set_visible"), api_def::gui::control_set_visible},
							{XOR("add_callback"), api_def::gui::control_add_callback}, {XOR("get_name"), api_def::gui::control_get_name},
							{XOR("get_type"), api_def::gui::control_get_type},});

	l.create_type(
		XOR("gui.list"), {{XOR("set_tooltip"), api_def::gui::control_set_tooltip}, {XOR("set_visible"), api_def::gui::control_set_visible},
						  {XOR("add_callback"), api_def::gui::control_add_callback}, {XOR("get_value"), api_def::gui::list_get_value},
						  {XOR("get"), api_def::gui::list_get_value}, {XOR("set_value"), api_def::gui::list_set_value}, {XOR("set"), api_def::gui::list_set_value},
						  {XOR("add"), api_def::gui::list_add}, {XOR("remove"), api_def::gui::list_remove}, {XOR("get_options"), api_def::gui::list_get_options},
						  {XOR("get_name"), api_def::gui::control_get_name}, {XOR("get_type"), api_def::gui::control_get_type},});

	l.create_type(
		XOR("render.anim_float"), {{XOR("direct"), api_def::render::anim_float_direct}, {XOR("get_value"), api_def::render::anim_float_get_value},
								   {XOR("get"), api_def::render::anim_float_get_value},});

	l.create_type(
		XOR("render.anim_color"), {{XOR("direct"), api_def::render::anim_color_direct}, {XOR("get_value"), api_def::render::anim_color_get_value},
								   {XOR("get"), api_def::render::anim_color_get_value},});

	/*l.create_type(XOR("csgo.netvar"), {
					  {XOR("__index"), api_def::net_prop::index},
					  {XOR("__newindex"), api_def::net_prop::new_index},
				  });

	l.create_type(XOR("csgo.entity"), {
					  {XOR("__index"), api_def::entity::index},
					  {XOR("__newindex"), api_def::entity::new_index},
				  });*/

	l.create_type(XOR("render.esp_flag"), {});

	/*l.create_type(XOR("csgo.entity"), {
					  {XOR("get_index"), api_def::entity::get_index},
					  {XOR("is_valid"), api_def::entity::is_valid},
					  {XOR("is_alive"), api_def::entity::is_alive},
					  {XOR("is_dormant"), api_def::entity::is_dormant},
					  {XOR("is_player"), api_def::entity::is_player},
					  {XOR("is_enemy"), api_def::entity::is_enemy},
					  {XOR("get_class"), api_def::entity::get_class},
					  {XOR("get_prop"), api_def::entity::get_prop},
					  {XOR("set_prop"), api_def::entity::set_prop},
					  {XOR("get_hitbox_position"), api_def::entity::get_hitbox_position},
					  {XOR("get_eye_position"), api_def::entity::get_eye_position},
					  {XOR("get_player_info"), api_def::entity::get_player_info},
					  {XOR("get_move_type"), api_def::entity::get_move_type},
					  {XOR("get_bbox"), api_def::entity::get_bbox},
					  {XOR("get_weapon"), api_def::entity::get_weapon},
					  {XOR("get_esp_alpha"), api_def::entity::get_esp_alpha},
				  });

	l.create_type(XOR("csgo.cvar"), {
					  {XOR("get_int"), api_def::cvar::get_int},
					  {XOR("get_float"), api_def::cvar::get_float},
					  {XOR("set_int"), api_def::cvar::set_int},
					  {XOR("set_float"), api_def::cvar::set_float},
					  {XOR("get_string"), api_def::cvar::get_string},
					  {XOR("set_string"), api_def::cvar::set_string},
				  });

	l.create_type(XOR("csgo.event"), {
					  {XOR("__index"), api_def::event::index},
					  {XOR("__newindex"), api_def::stub_new_index},
				  });

	l.create_type(XOR("csgo.user_cmd"), {
					  {XOR("get_command_number"), api_def::user_cmd::get_command_number},
					  {XOR("get_view_angles"), api_def::user_cmd::get_view_angles},
					  {XOR("get_move"), api_def::user_cmd::get_move},
					  {XOR("get_buttons"), api_def::user_cmd::get_buttons},
					  {XOR("set_view_angles"), api_def::user_cmd::set_view_angles},
					  {XOR("set_move"), api_def::user_cmd::set_move},
					  {XOR("set_buttons"), api_def::user_cmd::set_buttons},
				  });

	l.create_type(XOR("csgo.material"), {
					  {XOR("__index"), api_def::mat::index},
					  {XOR("__gc"), api_def::mat::gc},
				  });

	l.create_type(XOR("csgo.material_var"), {
					  {XOR("get_float"), api_def::mat::get_float},
					  {XOR("set_float"), api_def::mat::set_float},
					  {XOR("get_int"), api_def::mat::get_int},
					  {XOR("set_int"), api_def::mat::set_int},
					  {XOR("get_string"), api_def::mat::get_string},
					  {XOR("set_string"), api_def::mat::set_string},
					  {XOR("get_vector"), api_def::mat::get_vector},
					  {XOR("set_vector"), api_def::mat::set_vector},
				  });

	l.create_namespace(XOR("panorama"), {
						   {XOR("eval"), api_def::panorama::eval}
					   });*/

	l.create_namespace(
		XOR("fs"), {{XOR("read"), api_def::fs::read}, {XOR("read_stream"), api_def::fs::read_stream}, {XOR("write"), api_def::fs::write},
					{XOR("write_stream"), api_def::fs::write_stream}, {XOR("remove"), api_def::fs::remove}, {XOR("exists"), api_def::fs::exists},
					{XOR("is_file"), api_def::fs::is_file}, {XOR("is_dir"), api_def::fs::is_dir}, {XOR("create_dir"), api_def::fs::create_dir},});

	l.create_namespace(XOR("zip"), {{XOR("create"), api_def::zip::create}, {XOR("open"), api_def::zip::open}, {XOR("open_stream"), api_def::zip::open_stream},});

	l.create_type(
		XOR("utils.timer"), {{XOR("start"), api_def::timer::start}, {XOR("stop"), api_def::timer::stop}, {XOR("run_once"), api_def::timer::run_once},
							 {XOR("set_delay"), api_def::timer::set_delay}, {XOR("is_active"), api_def::timer::is_active}});

	/*l.create_type(XOR("vec3"), {
					  {XOR("__index"), api_def::math::vec3_index},
					  {XOR("__newindex"), api_def::math::vec3_new_index},
					  {XOR("__add"), api_def::math::vec3_add},
					  {XOR("__sub"), api_def::math::vec3_sub},
					  {XOR("__mul"), api_def::math::vec3_mul},
					  {XOR("__div"), api_def::math::vec3_div},
					  {XOR("calc_angle"), api_def::math::vec3_calc_angle},
					  {XOR("unpack"), api_def::math::vec3_unpack},
				  });*/

	l.create_type(
		XOR("zip"), {{XOR("__gc"), api_def::zip::gc}, {XOR("get_files"), api_def::zip::get_files}, {XOR("read"), api_def::zip::read},
					 {XOR("read_stream"), api_def::zip::read_stream}, {XOR("write"), api_def::zip::write}, {XOR("write_stream"), api_def::zip::write_stream},
					 {XOR("save"), api_def::zip::save}, {XOR("exists"), api_def::zip::exists}, {XOR("extract"), api_def::zip::extract},
					 {XOR("extract_all"), api_def::zip::extract_all},});
}

void lua::script::add_gui_element(uint64_t e_id) { gui_elements.emplace_back(e_id); }

void lua::script::add_gui_element_with_callback(uint64_t e_id)
{
	if (std::find(gui_callbacks.begin(), gui_callbacks.end(), e_id) == gui_callbacks.end())
		gui_callbacks.emplace_back(e_id);
}

void lua::script::add_font(uint64_t _id) { fonts.emplace_back(_id); }

void lua::script::add_texture(uint64_t _id) { textures.emplace_back(_id); }

void lua::script::add_animator(uint64_t _id) { animators.emplace_back(_id); }

void lua::script::add_shader(uint64_t _id) { shaders.emplace_back(_id); }

void lua::script::add_net_id(const std::string &_id) { net_ids.emplace_back(_id); }

void lua::script::run_timers() const
{
	if (!is_running || lua::helpers::timers[id].empty())
		return;

	auto &my_timers = lua::helpers::timers[id];

	// loop through all existing callbacks
	for (auto it = my_timers.begin(); it != my_timers.end();)
	{
		// get current callback
		const auto timer = *it;

		// skip if inactive
		if (!timer->is_active())
		{
			++it;
			continue;
		}

		// we waited long enough
		if (timer->cycle_completed())
		{
			// run callback
			const auto cbk = timer->get_callback();

			if (cbk)
				cbk();
		}

		// should be deleted
		if (timer->should_delete())
		{
			// erase callback
			it = my_timers.erase(it);
		}
		else
			++it;
	}
}
