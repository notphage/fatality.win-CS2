#pragma once

#include <macros.h>
#include <utils/value_obfuscation.h>
#include <utils/util.h>
#include <utils/mem.h>

#include "sdk/globalvars.h"
#include "sdk/math/vector.h"

namespace sdk
{
	struct global_vars_t;
	struct cgame_resource_service;
	struct cgame_entity_system;
	struct cengine_client;
	struct cs2_player_controller;
	struct ccsgo_input;
	struct cinput_system;
	struct cgame_ui_funcs;
	struct ccvar;
	struct clocalize;
	struct cui_engine;
	struct cmem_alloc;
	struct cprediction;

	namespace schema::client
	{
		class ccsweapon_base_vdata;
		class econ_item_view;
	}
}

struct game_t
{
	game_t(uintptr_t base, uint32_t tok);

	void init();
	void unload() const;

	using get_weapon_data_t = sdk::schema::client::ccsweapon_base_vdata*(*)(sdk::schema::client::econ_item_view *);
	using sdl_set_relative_mouse_mode_t = int (*)(int);
	using sdl_set_window_grab_t = int (*)(void *, int);
	using set_channel_verbosity_t = void (*)(sdk::logging_channel_id_t id, sdk::logging_severity_t severity);
	using register_logging_channel_t = void (*)(
		const char *channel_name, sdk::register_tags_func register_tags_func, int flags, sdk::logging_severity_t minimum_severity, sdk::color spew_color);
	using log_direct_t = sdk::logging_response_t(*)(sdk::logging_channel_id_t channel_id, sdk::logging_severity_t severity, sdk::color spew_color, const char *message);
	using find_channel_t = sdk::logging_channel_id_t (*)(const char *name);


	struct
	{
		get_weapon_data_t get_weapon_data;
		sdl_set_relative_mouse_mode_t sdl_set_relative_mouse_mode;
		sdl_set_window_grab_t sdl_set_window_grab;
		set_channel_verbosity_t set_channel_verbosity;
		register_logging_channel_t register_logging_channel;
		log_direct_t log_direct;
		find_channel_t find_channel;
	} fn;

	utils::library tier0, sdl3, gameoverlayrenderer64, client, engine2, inputsystem, localize, panorama, scenesystem;

	utils::encrypted_pptr<sdk::global_vars_t> global_vars;
	utils::encrypted_ptr<sdk::cgame_resource_service> resource_service;
	utils::encrypted_pptr<sdk::cgame_entity_system> game_entity_system;
	utils::encrypted_ptr<sdk::cengine_client> engine;
	utils::encrypted_pptr<sdk::cs2_player_controller> local_player_controller;
	utils::encrypted_pptr<sdk::ccsgo_input> input;
	utils::encrypted_pptr<sdk::cui_engine> ui_engine;
	utils::encrypted_ptr<sdk::cinput_system> input_system;
	utils::encrypted_ptr<sdk::cgame_ui_funcs> game_ui_funcs;
	utils::encrypted_ptr<sdk::ccvar> cvar;
	utils::encrypted_ptr<sdk::clocalize> loc;
	utils::encrypted_ptr<sdk::cmem_alloc> mem_alloc;
	utils::encrypted_ptr<sdk::cprediction> prediction;

	uintptr_t base;
	std::string version{xorstr_("1.0.0")};
	std::vector<HANDLE> mem_font_hadles = {};
	std::string game_dir{};

private:
	void load_fonts();
	void remove_fonts() const;
};

extern std::unique_ptr<game_t> game;
