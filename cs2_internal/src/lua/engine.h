#ifndef LUA_ENGINE_H
#define LUA_ENGINE_H

#include <lua/state.h>
#include <mutex>

#include "ren/types/animator.h"

class value;

namespace lua
{
	enum script_type
	{
		st_script,
		// regular script
		st_library,
		// library
		st_remote // workshop script
	};

	struct script_metadata
	{
		std::optional<std::string> name{};
		std::optional<std::string> description{};
		std::optional<std::string> author{};
	};

	struct script_file
	{
		script_type type{};
		std::string name{};
		script_metadata metadata{};
		bool should_load{};
		bool should_unload{};
		bool is_loading{};
		bool is_proprietary{};
		int id{};
		int last_update{};

		std::string get_name() const { return metadata.name.value_or(name); }

		std::string get_file_path() const;
		void parse_metadata();

		[[nodiscard]] uint32_t make_id() const;
	};

	class script
	{
	public:
		virtual ~script() { unload(); }

		bool initialize();
		virtual bool call_main();
		void unload();

		void create_forward(const char *n);
		bool has_forward(uint32_t hash);
		void call_forward(uint32_t hash, const std::function<int(state &)> &arg_callback = nullptr, int ret = 0,
						  const std::function<void(state &)> &callback = nullptr);

		void run_timers() const;

		void add_gui_element(uint64_t id);
		void add_gui_element_with_callback(uint64_t id);
		void add_font(uint64_t id);
		void add_texture(uint64_t id);
		void add_animator(uint64_t id);
		void add_shader(uint64_t id);
		void add_net_id(const std::string &id);

		uint32_t id{}; // hashed name
		std::string name{}; // script name
		std::string file{}; // file path

		struct
		{
			bool is_proprietary{};
			int id{};
			int last_update{};
		} remote{};

		std::atomic_bool did_error{};
		std::atomic_bool is_running{}; // determines if script should run
		script_type type{}; // script type
		state l{}; // state wrapper

	protected:
		std::unordered_map<uint32_t, std::string> forwards{}; // global function list
		std::vector<uint64_t> gui_elements{};
		std::vector<uint64_t> gui_callbacks{};
		std::vector<uint64_t> fonts{};
		std::vector<uint64_t> textures{};
		std::vector<uint64_t> animators{};
		std::vector<uint64_t> shaders{};
		std::vector<std::string> net_ids{};

		void find_forwards();
		void hide_globals();
		void register_namespaces();
		void register_globals();
		void register_types();
	};

	using script_t = std::shared_ptr<script>;

	struct ren_slot
	{
		static constexpr uint64_t first_free = 1;
		uint64_t last_occupied;
		std::vector<uint64_t> available;

		uint64_t get_empty_slot()
		{
			if (available.empty())
				return ++last_occupied;
			return available.front();
		}

		void on_free(uint64_t id) { available.emplace_back(id); }

		void on_take(uint64_t id)
		{
			if (const auto i = std::find(available.begin(), available.end(), id); i != available.end())
				available.erase(i);
		}
	};

	class engine
	{
	public:
		~engine() { scripts.clear(); }

		void refresh_scripts();
		void refresh_remote_scripts();

		void run_autoload();
		void enable_autoload(const script_file &file);
		void disable_autoload(const script_file &file);
		bool is_autoload_enabled(const script_file &file);
		void sync_autoload();

		bool run_script(const script_file &file, bool sounds = true);
		void stop_script(const script_file &file);
		void stop_script(uint32_t id);
		void stop_all();

		void run_timers();

		script_t find_by_state(lua_State *s);
		script_t find_by_id(uint32_t id);

		void create_callback(const char *n);
		void callback(uint32_t id, const std::function<int(state &)> &arg_callback = nullptr, int ret = 0,
					  const std::function<void(state &)> &callback = nullptr);

		bool has_any_script() const { return !script_files.empty(); }
		void for_each_script_name(const std::function<void(script_file &)> &fn);

		std::optional<std::reference_wrapper<script_file>> find_script_file(uint32_t id);

		bool exists(uint32_t id) { return scripts.find(id) != scripts.end() && scripts[id]->is_running; }

		bool in_render{};
		bool is_updating{};
		std::vector<uint32_t> autoload{};

		ren_slot texture;
		ren_slot font;
		ren_slot animator;
		ren_slot shader;

		std::optional<evo::ren::rect> last_clip;
		std::unordered_map<uint64_t, std::shared_ptr<evo::ren::anim_base>> anims;

	private:
		std::mutex access_mutex{};

		std::unordered_map<uint32_t, script_t> scripts{};
		std::vector<script_file> script_files{};
	};

	inline engine api{};
}

#endif // LUA_ENGINE_H
