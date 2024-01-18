#include <filesystem>
#include <game/cfg.h>
#include <game/draw_manager.h>
#include <game/game.h>
#include <game/hook_manager.h>
#include <lua/engine.h>
#include <ren/renderer.h>
#include <resources/smallest_pixel.h>
#include <sdk/cvar.h>
#include <sdk/engine.h>
#include <sdk/inputsystem.h>
#include <sdk/interface.h>
#include <utils/mem.h>

uint32_t utils::runtime_basis = 2166136261u;

std::unique_ptr<game_t> game;

namespace fs = std::filesystem;

game_t::game_t(uintptr_t _base, uint32_t tok) :
	base(_base)
{
	using namespace utils;
	tier0 = library(XOR("tier0.dll"));
	sdl3 = library(XOR("sdl3.dll"));
	gameoverlayrenderer64 = library(XOR("gameoverlayrenderer64.dll"));
	client = library(XOR("client.dll"));
	engine2 = library(XOR("engine2.dll"));
	inputsystem = library(XOR("inputsystem.dll"));
	localize = library(XOR("localize.dll"));
	panorama = library(XOR("panorama.dll"));
	scenesystem = library(XOR("scenesystem.dll"));

	game_dir = ENC2(util::get_game_dir());

	if (!fs::exists(DEC_INLINE(game_dir) + XOR("fatality")))
		fs::create_directories(DEC_INLINE(game_dir) + XOR("fatality"));
	if (!fs::exists(DEC_INLINE(game_dir) + XOR("fatality/scripts")))
		fs::create_directories(DEC_INLINE(game_dir) + XOR("fatality/scripts"));
	if (!fs::exists(DEC_INLINE(game_dir) + XOR("fatality/scripts/remote")))
		fs::create_directories(DEC_INLINE(game_dir) + XOR("fatality/scripts/remote"));
	if (!fs::exists(DEC_INLINE(game_dir) + XOR("fatality/scripts/lib")))
		fs::create_directories(DEC_INLINE(game_dir) + XOR("fatality/scripts/lib"));
}

void game_t::load_fonts()
{
	std::vector<unsigned char> v;

	DWORD n_fonts;
	mem_font_hadles.push_back(AddFontMemResourceEx(smallest_pixel.data(), smallest_pixel.size(), nullptr, &n_fonts));
	smallest_pixel.clear();
	auto vec = std::vector<unsigned char>(smallest_pixel);
	smallest_pixel.swap(vec);
}

void game_t::remove_fonts() const
{
	for (auto &handle : mem_font_hadles)
		RemoveFontMemResourceEx(handle);
}

void game_t::init()
{
	using namespace utils;

	// load interfaces
	global_vars = encrypted_pptr<sdk::global_vars_t>(client.at(sdk::offsets::globals::global_vars));
	resource_service = encrypted_ptr<sdk::cgame_resource_service>(engine2.at(sdk::offsets::interfaces::engine2::game_resource_service_client_v001));
	game_entity_system = encrypted_pptr<sdk::cgame_entity_system>(client.at(sdk::offsets::globals::game_entity_system));
	engine = encrypted_ptr<sdk::cengine_client>(engine2.at(sdk::offsets::interfaces::engine2::source2_engine_to_client001));
	local_player_controller = encrypted_pptr<sdk::cs2_player_controller>(client.at(sdk::offsets::globals::local_player_controller));
	input = encrypted_pptr<sdk::ccsgo_input>(client.at(sdk::offsets::globals::ccsgo_input));
	ui_engine = encrypted_pptr<sdk::cui_engine>(client.at(sdk::offsets::globals::panorama_ui));
	input_system = encrypted_ptr<sdk::cinput_system>(inputsystem.at(sdk::offsets::interfaces::inputsystem::input_system_version001));
	game_ui_funcs = encrypted_ptr<sdk::cgame_ui_funcs>(engine2.at(sdk::offsets::interfaces::engine2::vengine_gameuifuncs_version005));
	cvar = encrypted_ptr<sdk::ccvar>(tier0.at(sdk::offsets::interfaces::tier0::vengine_cvar007));
	loc = encrypted_ptr<sdk::clocalize>(localize.at(sdk::offsets::interfaces::localize::localize_001));
	mem_alloc = encrypted_ptr<sdk::cmem_alloc>(tier0.deref(sdk::offsets::globals::mem_alloc));
	prediction = encrypted_ptr<sdk::cprediction>(client.at(sdk::offsets::interfaces::client::source2_client_prediction001));

	fn.get_weapon_data = reinterpret_cast<get_weapon_data_t>(client.at(sdk::offsets::functions::client::get_weapon_data));
	fn.sdl_set_relative_mouse_mode = reinterpret_cast<sdl_set_relative_mouse_mode_t>(sdl3.at(sdk::offsets::functions::sdl3::set_relative_mouse_mode));
	fn.sdl_set_window_grab = reinterpret_cast<sdl_set_window_grab_t>(sdl3.at(sdk::offsets::functions::sdl3::set_window_grab));
	fn.set_channel_verbosity = reinterpret_cast<set_channel_verbosity_t>(tier0.at(sdk::offsets::functions::logging_system::set_channel_verbosity));
	fn.register_logging_channel = reinterpret_cast<register_logging_channel_t>(tier0.at(sdk::offsets::functions::logging_system::register_logging_channel));
	fn.log_direct = reinterpret_cast<log_direct_t>(tier0.at(sdk::offsets::functions::logging_system::log_direct));
	fn.find_channel = reinterpret_cast<find_channel_t>(tier0.at(sdk::offsets::functions::logging_system::find_channel));

	fn.set_channel_verbosity(fn.find_channel("Shooting"), sdk::logging_severity_t::LS_HIGHEST_SEVERITY);

	load_fonts();

	lua::api.refresh_scripts();

	// load other
	cfg.init();

#ifdef _DEBUG
	game->cvar->unlock();
#endif

	hook_manager.init();
	hook_manager.attach();
}

void game_t::unload() const
{
	std::this_thread::sleep_for(std::chrono::milliseconds(200));

	hook_manager.detach();

	std::this_thread::sleep_for(std::chrono::milliseconds(200));

	draw_mgr.destroy_objects();
	game->input_system->set_input(true);
	evo::ren::draw.destroy_objects();
	evo::ren::draw = {};

	FreeLibrary(reinterpret_cast<HMODULE>(base));
	FreeLibraryAndExitThread(reinterpret_cast<HMODULE>(base), 0);
}
