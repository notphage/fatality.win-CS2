#include <filesystem>
#include <fstream>
#include <iomanip>
#include "macros.h"
#include "lua/state.h"
#include "lua/api_def.h"
#include "lua/helpers.h"
#include "utils/util.h"

int lua::api_def::database::save(lua_State *l)
{
	runtime_state s(l);

	if (!s.is_string(1) || (!s.is_string(2) && !s.is_table(2)))
	{
		s.error(XOR("usage: database.save(filename, string/table)"));
		return 0;
	}

	auto filename = std::string(s.get_string(1));
	if (filename.find("..") != std::string::npos || filename.find(':') != std::string::npos || filename.find('/') != std::string::npos || filename.find('\\') !=
		std::string::npos)
	{
		s.error(XOR("usage: database.save(filename, string/table): filename can not be an absolute path"));
		return 0;
	}

	if (!std::filesystem::exists(DEC_INLINE(game->game_dir) + XOR("fatality/database")))
		std::filesystem::create_directories(DEC_INLINE(game->game_dir) + XOR("fatality/database"));

	std::ofstream o(DEC_INLINE(game->game_dir) + XOR("fatality/database/") + filename);

	if (s.is_table(2))
	{
		auto parsed = helpers::parse_table(l, 2);
		o << std::setw(4) << parsed << std::endl;
	}
	else
		o << s.get_string(2);

	o.close();

	return 0;
}

int lua::api_def::database::load(lua_State *l)
{
	runtime_state s(l);

	if (!s.is_string(1))
	{
		s.error(XOR("usage: database.load(filename)"));
		return 0;
	}

	auto filename = std::string(DEC_INLINE(game->game_dir) + s.get_string(1));
	if (filename.find("..") != std::string::npos || filename.find(':') != std::string::npos || filename.find('/') != std::string::npos || filename.find('\\') !=
		std::string::npos)
	{
		s.error(XOR("database.load(filename): filename can not be an absolute path"));
		return 0;
	}

	std::ifstream i(DEC_INLINE(game->game_dir) + XOR("fatality/database/") + filename);

	if (!i.good())
		return 0;

	std::string content((std::istreambuf_iterator<char>(i)), std::istreambuf_iterator<char>());

	if (content.length() == 0)
		return 0;

	json j = json::parse(content, nullptr, false);
	if (j.is_discarded())
	{
		s.push(content.c_str());
		return 1;
	}

	return helpers::load_table(l, j);
}
