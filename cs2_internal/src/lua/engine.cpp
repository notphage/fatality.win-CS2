#include <filesystem>
#include <fstream>
#include "engine_net.h"
#include "engine.h"
#include "macros.h"
#include <utils/json.hpp>

#undef min
#undef max
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#define MINIZ_HEADER_FILE_ONLY
#include <utils/zip_file.hpp>

#include "helpers.h"
#include "game/cfg.h"
#include "gui/gui.h"
#include "menu/menu.h"
#include "utils/util.h"

namespace lua
{
	engine_net_data_t net_data;
}

namespace fs = std::filesystem;

void lua::engine::refresh_scripts()
{
	//VIRTUALIZER_TIGER_WHITE_START;
	script_files.clear();

	refresh_remote_scripts();
	for (auto &f : fs::directory_iterator(DEC_INLINE(game->game_dir) + XOR("fatality/scripts")))
	{
		if (f.path().extension() != XOR(".lua"))
			continue;

		script_file file{st_script, f.path().filename().replace_extension(XOR("")).string()};

		file.parse_metadata();
		script_files.emplace_back(file);

	}

	for (auto &f : fs::directory_iterator(DEC_INLINE(game->game_dir) + XOR("fatality/scripts/lib")))
	{
		if (f.path().extension() != XOR(".lua"))
			continue;

		script_file file{st_library, f.path().filename().replace_extension(XOR("")).string()};

		file.parse_metadata();
		script_files.emplace_back(file);
	}

	// remove scripts that are no longer in the list.
	std::lock_guard _lock(access_mutex);
	for (const auto &script : scripts)
	{
		const auto p = std::find_if(script_files.begin(), script_files.end(), [&](const script_file &f) { return f.make_id() == script.first; });

		if (p != script_files.end())
			continue;

		scripts[script.first]->unload();
	}
	//VIRTUALIZER_TIGER_WHITE_END;
}

namespace script_db
{
	struct cache_data_t
	{
		std::string filename{};
		bool is_proprietary{};
		int id{};
		int last_update{};

		std::string name{};
		std::string author{};
		std::string description{};

		bool is_library{};
	};

	std::vector<cache_data_t> read()
	{
		// read file
		std::vector<cache_data_t> data;

		const auto path = std::string(DEC_INLINE(game->game_dir) + XOR("fatality/scripts/remote/cache.json"));
		std::ifstream file(path, std::ios::binary);

		if (!file.is_open())
			return data;

		// parse file
		nlohmann::json j;
		file >> j;

		for (const auto &i : j)
		{
			cache_data_t d{i[XOR("filename")], i[XOR("is_proprietary")], i[XOR("id")], i[XOR("last_update")], i[XOR("name")], i[XOR("author")], i[XOR("description")],
						   i[XOR("is_library")]};
			data.emplace_back(d);
		}

		return data;
	}

	void write(const std::vector<cache_data_t> &data)
	{
		// write file
		const auto path = std::string(DEC_INLINE(game->game_dir) + XOR("fatality/scripts/remote/cache.json"));
		std::ofstream file(path, std::ios::binary);

		if (!file.is_open())
			return;

		// parse file
		nlohmann::json j;
		for (const auto &i : data)
		{
			nlohmann::json d{{XOR("filename"), i.filename}, {XOR("is_proprietary"), i.is_proprietary}, {XOR("id"), i.id}, {XOR("last_update"), i.last_update},
							 {XOR("name"), i.name}, {XOR("author"), i.author}, {XOR("description"), i.description}, {XOR("is_library"), i.is_library}};
			j.push_back(d);
		}

		file << j;
	}
} // namespace script_db

void lua::engine::refresh_remote_scripts()
{
#ifdef CS2_CLOUD
	VIRTUALIZER_TIGER_WHITE_START;

	using namespace networking;

	net_data.has_key = false;
	conn->send_get_script_key();
	while (!net_data.has_key)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
		std::this_thread::yield();
	}

	// read local cache
	auto cache = script_db::read();

	const auto add_files = [&cache, this]()
	{
		for (const auto &c : cache)
		{
			if (c.is_library)
				continue;

			if (!fs::exists(DEC_INLINE(game->game_dir) + XOR("fatality/scripts/remote/") + c.filename))
				continue;

			script_file f{lua::st_remote, c.filename.substr(0, c.filename.find(XOR(".lua")))};

			f.is_proprietary = c.is_proprietary;
			f.id = c.id;
			f.last_update = c.last_update;
			f.metadata.name = c.name;
			f.metadata.description = c.description;
			f.metadata.author = c.author;

			script_files.push_back(f);
		}
	};

	// prepare delta
	std::vector<msg::script_cached_info> delta;
	for (const auto &c : cache)
		delta.push_back({c.id, c.last_update});

	net_data.has_data = false;
	conn->send_refresh_scripts(delta);
	while (!net_data.has_data)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
		std::this_thread::yield();
	}

	auto &delta_in = net_data.delta;
	if (delta_in.empty())
	{
		add_files();
		return;
	}

	for (const auto &d : delta_in)
	{
		auto it = std::find_if(cache.begin(), cache.end(), [&](const auto &c) { return c.id == d.id; });
		if (d.should_erase)
		{
			// remove from cache
			if (it != cache.end())
			{
				// delete from disk
				const auto path =
					std::string(DEC_INLINE(game->game_dir) + XOR("fatality/scripts/") +
								std::string(d.is_library ? XOR("lib") : XOR("remote")) +
						XOR("/") + it->filename);
				if (fs::exists(path))
					fs::remove(path.c_str());
				cache.erase(it);
			}
		}
		else
		{
			const auto filename = d.is_library ? d.filename : (std::to_string(d.id) + XOR(".lua"));

			// check if not in the cache yet or needs update
			if (it == cache.end() || it->is_proprietary != d.is_proprietary || it->last_update != d.last_update)
			{
				// special case if it's a zip file
				if (d.filename.ends_with(XOR("zip")))
				{
					std::stringstream ss(d.content);
					miniz_cpp::zip_file zip(ss);

					// malformed package.
					if (!zip.has_file(XOR("scripts/remote/script.lua")))
						continue;

					for (const auto &n : zip.infolist())
					{
						fs::create_directories(
							fs::path(DEC_INLINE(game->game_dir) + XOR("fatality/") + n.filename).remove_filename());

						// encrypt lua file if it's proprietary
						auto content = zip.read(n);
						if (n.filename.ends_with(XOR("lua")) && n.filename.find(XOR("scripts/lib/")) != 0 &&
							d.is_proprietary)
							content = util::aes256_encrypt(content, net_data.enc_key);

						auto tmp_filename = n.filename;
						if (tmp_filename == XOR("scripts/remote/script.lua"))
							tmp_filename = XOR("scripts/remote/") + filename;

						// write file
						const auto path = std::string(DEC_INLINE(game->game_dir) + XOR("fatality/") + tmp_filename);
						if (path.find(XOR("fatality/scripts/lib/")) != 0 ||
							!fs::exists(path))
						{
							std::ofstream file(path, std::ios::binary);
							if (!file.is_open())
								continue;

							file << content;
						}
					}
				}
				else
				{
					const auto path = DEC_INLINE(game->game_dir) + XOR("fatality/scripts/") +
						std::string(d.is_library ? XOR("lib") : XOR("remote"))
						+
						XOR("/") + filename;

					// if proprietary then fetch encryption key
					if (d.is_proprietary)
					{
						// write script to file
						std::ofstream file(path, std::ios::binary);
						if (!file.is_open())
							continue;

						// encrypt script
						// p.s. this should really be serversided but i mean
						// not yet : )
						file << util::aes256_encrypt(d.content, net_data.enc_key);
					}
					else
					{
						// write script to file
						std::ofstream file(path, std::ios::binary);
						if (!file.is_open())
							continue;

						file << d.content;
					}
				}

				// add to cache
				if (it == cache.end())
				{
					cache.push_back({filename, d.is_proprietary, d.id, d.last_update, d.name, d.author, d.description,
									 d.is_library});
				}
				else
				{
					// update cache
					it->is_proprietary = d.is_proprietary;
					it->last_update = d.last_update;
					it->name = d.name;
					it->author = d.author;
					it->description = d.description;
				}
			}
		}
	}

	// write cache
	add_files();
	script_db::write(cache);

	VIRTUALIZER_TIGER_WHITE_END;
#endif
}

void lua::engine::run_autoload()
{
	//VIRTUALIZER_TIGER_WHITE_START;
	autoload.clear();
	for (auto &i : cfg.autoload.data)
		autoload.push_back(i);

	std::vector<uint32_t> unload{};
	for (const auto &[id, s] : scripts)
	{
		if (std::find(autoload.begin(), autoload.end(), id) == autoload.end())
			unload.emplace_back(id);
	}

	for (const auto &id : unload)
	{
		if (const auto f = api.find_script_file(id); f.has_value())
			f->get().should_unload = true;
		else
			stop_script(id);
	}

	for (const auto &f_id : autoload)
	{
		const auto file = std::find_if(script_files.begin(), script_files.end(), [f_id](const script_file &f) { return f.make_id() == f_id; });

		if (file == script_files.end())
			continue;

		const auto scr = find_by_id(file->make_id());
		if (file != script_files.end() && (!scr || !scr->is_running))
		{
			run_script(*file, false);
			evo::gui::notify.add(
				std::make_shared<evo::gui::notification>(XOR("Autoload"), XOR("Loaded ") + file->get_name(), evo::ren::draw.textures[GUI_HASH("icon_scripts")]));
		}
	}

	std::lock_guard lock(menu::men.scripts_mtx);
	const auto script_list = evo::gui::ctx->find<evo::gui::list>(GUI_HASH("scripts>general>list"));
	if (script_list)
		script_list->for_each_control(
			[&](std::shared_ptr<evo::gui::control> &c)
			{
				const auto sel = c->as<evo::gui::selectable>();
				if (lua::api.exists(sel->id))
					sel->is_loaded = true;
				else
					sel->is_loaded = false;

				sel->reset();
			});
	//VIRTUALIZER_TIGER_WHITE_END;
}

__attribute__( ( always_inline ) ) void lua::engine::enable_autoload(const script_file &file)
{
	if (std::find(autoload.begin(), autoload.end(), file.make_id()) != autoload.end())
		return;

	autoload.emplace_back(file.make_id());
	sync_autoload();
}

__attribute__( ( always_inline ) ) void lua::engine::disable_autoload(const script_file &file)
{
	if (const auto f = std::find(autoload.begin(), autoload.end(), file.make_id()); f != autoload.end())
		autoload.erase(f);

	sync_autoload();
}

__attribute__( ( always_inline ) ) bool lua::engine::is_autoload_enabled(const script_file &file)
{
	return std::find(autoload.begin(), autoload.end(), file.make_id()) != autoload.end();
}

__attribute__( ( always_inline ) ) void lua::engine::sync_autoload()
{
	cfg.autoload.data.clear();
	for (auto &i : autoload)
		cfg.autoload.data.push_back(i);
}

__attribute__( ( always_inline ) ) bool lua::engine::run_script(const script_file &file, bool sounds)
{
	if (file.type == st_library)
		return false;

	std::lock_guard _lock(access_mutex);

	// make sure file still exists
	if (!fs::exists(file.get_file_path()))
	{
		lua::helpers::error(XOR("Script error"), XOR("File not found."));

		return false;
	}

	// delete previously running script
	const auto id = file.make_id();
	if (scripts.find(id) != scripts.end())
		scripts.erase(id);

	// create script (lua will auto-initialize)
	const auto s = std::make_shared<script>();
	s->name = file.name;
	s->id = id;
	s->type = file.type;
	s->file = file.get_file_path();
	s->remote.is_proprietary = file.is_proprietary;
	s->remote.last_update = file.last_update;
	s->remote.id = file.id;

	// add to the list
	scripts[s->id] = s;

	// init the script
	if (!s->initialize() || !s->call_main())
	{
		// show the error message to user
		lua::helpers::error(XOR("Script error"), XOR("Unable to initialize script."));

		s->is_running = false;
		s->unload();

		scripts.erase(s->id);
		return false;
	}

	// mark as running
	s->is_running = true;

	//if (sounds)
	//EMIT_SUCCESS_SOUND();

	return true;
}

__declspec( noinline ) void lua::engine::stop_script(const script_file &file)
{
	//VIRTUALIZER_TIGER_WHITE_START;
	stop_script(file.make_id());
	//VIRTUALIZER_TIGER_WHITE_END;
}

__attribute__( ( always_inline ) ) void lua::engine::stop_script(uint32_t id)
{
	std::lock_guard _lock(access_mutex);
	if (const auto s = scripts.find(id); s != scripts.end() && s->second)
	{
		if (!s->second->did_error)
			s->second->call_forward(FNV1A("on_shutdown"));

		s->second->is_running = false;
		s->second->unload();
		scripts.erase(id);
	}
}

__attribute__( ( always_inline ) ) void lua::engine::for_each_script_name(const std::function<void(script_file &)> &fn)
{
	for (auto &f : script_files)
	{
		if (f.type != st_library)
			fn(f);
	}
}

std::optional<std::reference_wrapper<lua::script_file>> lua::engine::find_script_file(uint32_t id)
{
	const auto p = std::ranges::find_if(
		script_files, [id](const script_file &f)
		{
			return f.make_id() == id;
		});

	if (p != script_files.end())
		return std::ref(*p);

	return {};
}

void lua::engine::callback(uint32_t id, const std::function<int(state &)> &arg_callback, int ret, const std::function<void(state &)> &callback)
{
	try
	{
		std::lock_guard _lock(access_mutex);
		for (const auto &[_, s] : scripts)
		{
			if (s->is_running)
				s->call_forward(id, arg_callback, ret, callback);
		}
	}
	catch (...) {}
}

void lua::engine::create_callback(const char *n)
{
	std::lock_guard _lock(access_mutex);
	for (const auto &[_, s] : scripts)
	{
		if (!s->has_forward(utils::fnv1a(n)))
			s->create_forward(n);
	}
}

void lua::engine::stop_all()
{
	std::lock_guard _lock(access_mutex);
	scripts.clear();
}

void lua::engine::run_timers()
{
	std::lock_guard _lock(access_mutex);
	for (const auto &[_, s] : scripts)
	{
		if (s->is_running)
			s->run_timers();
	}
}

lua::script_t lua::engine::find_by_state(lua_State *state)
{
	for (const auto &[_, s] : scripts)
	{
		if (s->l.get_state() == state)
			return s;
	}

	return nullptr;
}

lua::script_t lua::engine::find_by_id(uint32_t id)
{
	for (const auto &[_, s] : scripts)
	{
		if (_ == id)
			return s;
	}

	return nullptr;
}

uint32_t lua::script_file::make_id() const
{
	return utils::fnv1a(name.c_str()) ^ type;
}

__declspec( noinline ) std::string lua::script_file::get_file_path() const
{
	//VIRTUALIZER_TIGER_WHITE_START;
	std::string base_path{DEC_INLINE(game->game_dir) + XOR("fatality/")};
	switch (type)
	{
	case st_script:
		base_path += XOR("scripts/");
		break;
	case st_remote:
		base_path += XOR("scripts/remote/");
		break;
	case st_library:
		base_path += XOR("scripts/lib/");
		break;
	default:
		return XOR("");
	}

	const auto ret = base_path + name + XOR(".lua");

	//VIRTUALIZER_TIGER_WHITE_END;

	return ret;
}

__attribute__( ( always_inline ) ) void lua::script_file::parse_metadata()
{
	std::ifstream file(get_file_path());
	if (!file.is_open())
		return;

	auto runs = 4;

	std::string line{};
	while (std::getline(file, line))
	{
		if (runs-- == 0)
			break;

		// check our own shebang notation
		if (line.find(XOR("--.")) != 0)
			continue;

		// remove shebang
		line = line.erase(0, 3);

		// split in parts
		const auto parts = util::split(line, XOR(" "));
		if (parts.empty())
			continue;

		const auto item = utils::fnv1a(parts[0].c_str());
		switch (item)
		{
		case FNV1A("name"):
			metadata.name = line.size() > 4 ? line.substr(5) : std::string();
			break;
		case FNV1A("description"):
			metadata.description = line.size() > 11 ? line.substr(12) : std::string();
			break;
		case FNV1A("author"):
			metadata.author = line.size() > 6 ? line.substr(7) : std::string();
			break;
		default:
			break;
		}
	}
}
