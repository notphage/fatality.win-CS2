#include "helpers.h"

#include <functional>
#include <unordered_map>

#include "macros.h"
#include "game/game.h"
#include "ren/types/color.h"
#include "utils/fnv1a.h"
#include <sdk/sdk.h>

namespace lua::helpers
{
	//std::unordered_map<uint32_t, std::unordered_map<uint32_t, var>> netvar_cache;

	//void cache_netvars(ClientClass *cc, RecvTable *table, size_t offset)
	//{
	//	if (!table)
	//		table = cc->m_pRecvTable;

	//	for (int i{}; i < table->m_nProps; ++i)
	//	{
	//		const auto cur = &table->m_pProps[i];
	//		if (cur->m_RecvType == dpt_datatable)
	//			cache_netvars(cc, cur->m_pDataTable, offset + cur->m_Offset);
	//		if (isdigit(cur->m_pVarName[0]))
	//			continue;

	//		// fix type
	//		auto type = cur->m_RecvType;
	//		if (type == dpt_datatable)
	//			type = dpt_int;

	//		bool is_bool{};
	//		bool is_short{};
	//		if (type == dpt_int)
	//		{
	//			std::string name{cur->m_pVarName};
	//			if (name.starts_with(XOR("m_b")))
	//				is_bool = true;
	//			else if (name.starts_with(XOR("m_f")) && name != XOR("m_fFlags"))
	//				type = dpt_float;
	//			else if (name.starts_with(XOR("m_v")) || name.starts_with(XOR("m_ang")))
	//				type = dpt_vector;
	//			else if (name.starts_with(XOR("m_psz")) || name.starts_with(XOR("m_sz")))
	//				type = dpt_string;
	//			else if (name == XOR("m_iItemDefinitionIndex"))
	//				is_short = true;
	//		}

	//		// see if it is an array
	//		bool is_array{};
	//		if (cur->m_RecvType == dpt_datatable && cur->m_pDataTable->m_nProps > 0)
	//		{
	//			const auto first = &cur->m_pDataTable->m_pProps[0];
	//			if (first->m_pVarName[0] == '0')
	//				is_array = true;
	//		}

	//		netvar_cache[utils::fnv1a(cc->m_pNetworkName)][utils::fnv1a(cur->m_pVarName)] = var{
	//			size_t(cur->m_Offset) + offset, (send_prop_type)type,
	//			is_bool, is_short, is_array};
	//	}
	//}

	/*var get_netvar(const char *name, ClientClass *cc)
	{
		const auto tab_hash = utils::fnv1a(cc->m_pNetworkName);
		if (!netvar_cache.contains(tab_hash) || netvar_cache[tab_hash].empty())
			cache_netvars(cc);

		if (netvar_cache.contains(tab_hash))
		{
			if (const auto name_hash = utils::fnv1a(name); netvar_cache[tab_hash].contains(name_hash))
				return netvar_cache[tab_hash][name_hash];
		}

		return {};
	}*/

	/*void state_set_prop(const lua_var &v, runtime_state &s, int off)
	{
		const auto addr = uint32_t(v.ent) + v.v.offset;
		switch (v.v.type)
		{
		default:
		case dpt_int:
			if (v.v.is_bool)
				*(bool *)addr = s.get_boolean(off);
			else if (v.v.is_short)
				*(short *)addr = (short)s.get_integer(off);
			else
				*(int *)addr = s.get_integer(off);
			break;
		case dpt_string:
			break;
		case dpt_vector:
			if (!s.is_number(off))
			{
				const auto vec = s.user_data<Vector>(off);
				*(float *)addr = vec.x;
				((float *)addr)[1] = vec.y;
				((float *)addr)[2] = vec.z;
			}
			else
			{
				*(float *)addr = s.get_float(off);
				((float *)addr)[1] = s.get_float(off + 1);
				((float *)addr)[2] = s.get_float(off + 2);
			}

			break;
		case dpt_float:
			*(float *)addr = s.get_float(off);
			break;
		case dpt_int64:
			break;
		case dpt_vectorxy:
			if (!s.is_number(off))
			{
				const auto vec = s.user_data<Vector>(off);
				*(float *)addr = vec.x;
				((float *)addr)[1] = vec.y;
			}
			else
			{
				*(float *)addr = s.get_float(off);
				((float *)addr)[1] = s.get_float(off + 1);
			}

			break;
		}
	}

	int state_get_prop(const lua_var &v, runtime_state &s)
	{
		const auto addr = uint32_t(v.ent) + v.v.offset;
		switch (v.v.type)
		{
		default:
		case dpt_int:
			if (v.v.is_bool)
				s.push(*(bool *)addr);
			else if (v.v.is_short)
				s.push(*(short *)addr);
			else
				s.push(*(int *)addr);

			return 1;
		case dpt_string:
			s.push((const char *)addr);
			return 1;
		case dpt_vector:
		{
			s.create_user_object(XOR("vec3"), (Vector *)addr);
			return 1;
		}
		case dpt_float:
			s.push(*(float *)addr);
			return 1;
		case dpt_int64:
			s.push(std::to_string(*(int64_t *)addr).c_str());
			return 1;
		case dpt_vectorxy:
		{
			Vector vec{*(float *)addr, ((float *)addr)[1], 0.f};
			s.create_user_object(XOR("vec3"), &vec);
			return 1;
		}
		}
	}*/

	uintptr_t get_interface(const uint32_t module, const uint32_t name)
	{
		const auto list = **reinterpret_cast<interface_reg ***>(find_pattern(find_module(module), XOR("55 8B EC 56 8B 35 ? ? ? ? 57 85 F6 74 38 8B"), 6).address);

		for (auto current = list; current; current = current->next)
			if (utils::fnv1a(current->name) == name)
				return reinterpret_cast<uintptr_t>(current->create());

		return -1;
	}

	namespace scanner
	{
		enum element_type
		{
			el_byte,
			el_any,
			el_ref_open,
			el_ref_close,
			el_follow,
			el_offset,
			el_add,
			el_sub,
			el_max
		};

		struct element_t
		{
			size_t pos;
			element_type type;
			uintptr_t value;
		};

		struct deref_t
		{
			uintptr_t pos;
			uintptr_t offset;
			uintptr_t length;
			bool should_follow;
		};
	}

	scan_result find_pattern(const uint8_t *module, const char *pat, const ptrdiff_t off)
	{
		const auto module_size = PIMAGE_NT_HEADERS(module + PIMAGE_DOS_HEADER(module)->e_lfanew)->OptionalHeader.SizeOfImage;
		std::string pattern{pat};

		// parse pattern.
		std::vector<scanner::element_t> elements;
		int open_count{}, close_count{};
		size_t pos{};
		for (auto it = pattern.begin(); it != pattern.end(); ++it)
		{
			switch (*it)
			{
			case ' ':
				continue;
			case '?':
				if (it + 1 != pattern.end() && *(it + 1) == '?')
					++it;

				elements.push_back({pos++, scanner::el_any});
				continue;
			case '[':
				++open_count;
				elements.push_back({pos++, scanner::el_ref_open});
				continue;
			case ']':
				++close_count;
				elements.push_back({pos++, scanner::el_ref_close});
				continue;
			case '*':
				elements.push_back({pos++, scanner::el_follow});
				continue;
			case '+':
				elements.push_back({pos++, scanner::el_add});
				continue;
			case '-':
				elements.push_back({pos++, scanner::el_sub});
				continue;
			}

			if (!elements.empty())
			{
				const auto &last = elements.back();
				if (isdigit(*it) && (last.type == scanner::el_add || last.type == scanner::el_sub))
				{
					uintptr_t offset{};
					while (it != pattern.end() && isdigit(*it))
					{
						offset *= 10;
						offset += *it - '0';
						++it;
					}

					if (last.type == scanner::el_sub)
						offset = -offset;

					--it;
					elements.push_back({pos++, scanner::el_offset, offset});
					continue;
				}
			}

			if (isxdigit(*it))
			{
				uintptr_t value{};
				while (it != pattern.end() && isxdigit(*it))
				{
					value *= 16;
					value += isdigit(*it) ? *it - '0' : tolower(*it) - 'a' + 10;
					++it;
				}

				--it;
				elements.push_back({pos++, scanner::el_byte, value});
				continue;
			}

			return {0, scan_result::parse_error, pos};
		}

		if (open_count != close_count)
			return {0, scan_result::parse_error};

		if (elements.empty())
			return {0, scan_result::not_found};

		// build dereference stack.
		std::vector<scanner::deref_t> deref_stack;
		bool had_follow{};
		int skip_closes, old_open_count{open_count}, length_delta{open_count * 2 - 1};
		size_t last_byte{};
		for (const auto &elf : elements)
		{
			if (elf.type == scanner::el_byte || elf.type == scanner::el_any)
				last_byte = elf.pos + 1;

			if (elf.type == scanner::el_follow)
			{
				had_follow = true;
				continue;
			}

			if (elf.type != scanner::el_ref_open)
			{
				if (had_follow)
					return {0, scan_result::parse_error, elf.pos};

				continue;
			}

			scanner::deref_t cur_deref{};
			cur_deref.should_follow = had_follow;
			cur_deref.pos = last_byte;
			last_byte = 0;

			skip_closes = old_open_count - open_count;

			for (auto it = elements.rbegin(); it != elements.rend(); ++it)
			{
				auto &elb = *it;
				if (elb.type != scanner::el_ref_close || skip_closes--)
					continue;

				cur_deref.length = elb.pos - elf.pos - length_delta;

				if (elb.pos + 2 < elements.size())
				{
					const auto &elc = elements[elb.pos + 1];
					if (elc.type == scanner::el_add || elc.type == scanner::el_sub)
						cur_deref.offset = elements[elb.pos + 2].value;
				}

				break;
			}

			if (!cur_deref.length)
				return {0, scan_result::too_small_deref, elf.pos};

			had_follow = false;
			deref_stack.push_back(cur_deref);
			--open_count;
			length_delta -= 2;
		}

		// build scan pattern.
		std::vector<scanner::element_t> scan_pattern;
		for (const auto &el : elements)
		{
			if (el.type == scanner::el_byte || el.type == scanner::el_any)
				scan_pattern.push_back(el);
		}

		scan_result result{0, scan_result::not_found};
		for (auto i = (uintptr_t)module; i < (uintptr_t)module + module_size - scan_pattern.size(); ++i)
		{
			bool match{true};
			size_t cur{};
			for (const auto &el : scan_pattern)
			{
				++cur;
				if (el.type != scanner::el_any && *(uint8_t *)(i + (cur - 1)) != (uint8_t)el.value)
				{
					match = false;
					break;
				}
			}

			if (match)
			{
				result.address = (uintptr_t)i;
				result.error = scan_result::found;
			}
		}

		if (result.error != scan_result::found)
			return result;

		// apply the stack.
		for (auto it = deref_stack.rbegin(); it != deref_stack.rend(); ++it)
		{
			auto &deref = *it;
			if (deref.should_follow)
			{
				if (deref.length < 4)
				{
					result.error = scan_result::too_small_deref;
					result.error_pos = deref.pos;
					return result;
				}

				try
				{
					result.address = *reinterpret_cast<uintptr_t *>(result.address + deref.pos) + result.address + sizeof(uintptr_t) + 1 + deref.offset;
				}
				catch (...)
				{
					result.error = scan_result::exception;
					return result;
				}

				continue;
			}

			try
			{
				switch (deref.length)
				{
				case 1:
					result.address = *reinterpret_cast<uint8_t *>(result.address + deref.pos);
					break;
				case 2:
					result.address = *reinterpret_cast<uint16_t *>(result.address + deref.pos);
					break;
				case 4:
					result.address = *reinterpret_cast<uint32_t *>(result.address + deref.pos);
					break;
				default:
					result.error = scan_result::not_a_power;
					return result;
				}
			}
			catch (...)
			{
				result.error = scan_result::exception;
				return result;
			}
		}

		result.address += off;
		return result;
	}

	typedef struct _LDR_MODULE
	{
		LIST_ENTRY InLoadOrderModuleList;
		LIST_ENTRY InMemoryOrderModuleList;
		LIST_ENTRY InInitializationOrderModuleList;
		PVOID BaseAddress;
		PVOID EntryPoint;
		ULONG SizeOfImage;
		UNICODE_STRING FullDllName;
		UNICODE_STRING BaseDllName;
		ULONG Flags;
		SHORT LoadCount;
		SHORT TlsIndex;
		LIST_ENTRY HashTableEntry;
		ULONG TimeDateStamp;
	} LDR_MODULE, *PLDR_MODULE;

	typedef struct _PEB_LOADER_DATA
	{
		ULONG Length;
		BOOLEAN Initialized;
		PVOID SsHandle;
		LIST_ENTRY InLoadOrderModuleList;
		LIST_ENTRY InMemoryOrderModuleList;
		LIST_ENTRY InInitializationOrderModuleList;
	} PEB_LOADER_DATA, *PPEB_LOADER_DATA;


	const uint8_t *find_module(const uint32_t hash)
	{
		static auto file_name_w = [](wchar_t *path)
		{
			wchar_t *slash = path;

			while (path && *path)
			{
				if ((*path == '\\' || *path == '/' || *path == ':') && path[1] && path[1] != '\\' && path[1] != '/')
					slash = path + 1;
				path++;
			}

			return slash;
		};

		const auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

		if (!peb)
			return nullptr;

		const auto ldr = reinterpret_cast<PPEB_LOADER_DATA>(peb->Ldr);

		if (!ldr)
			return nullptr;

		const auto head = &ldr->InLoadOrderModuleList;
		auto current = head->Flink;

		while (current != head)
		{
			const auto module = CONTAINING_RECORD(current, LDR_MODULE, InLoadOrderModuleList);
			std::wstring wide(file_name_w(module->FullDllName.Buffer));
			std::string name(wide.begin(), wide.end());
			std::transform(name.begin(), name.end(), name.begin(), ::tolower);

			if (utils::fnv1a(name.c_str()) == hash)
				return reinterpret_cast<uint8_t *>(module->BaseAddress);

			current = current->Flink;
		}

		return nullptr;
	}

	json parse_table(lua_State *L, int index)
	{
		runtime_state s(L);

		json root;

		// Push another reference to the table on top of the stack (so we know
		// where it is, and this function can work for negative, positive and
		// pseudo indices
		lua_pushvalue(L, index);
		// stack now contains: -1 => table
		lua_pushnil(L);
		// stack now contains: -1 => nil; -2 => table
		while (lua_next(L, -2))
		{
			// stack now contains: -1 => value; -2 => key; -3 => table
			// copy the key so that lua_tostring does not modify the original
			lua_pushvalue(L, -2);
			// stack now contains: -1 => key; -2 => value; -3 => key; -4 => table

			json sub;

			// set value by type
			switch (lua_type(L, -2))
			{
			case LUA_TNIL:
				sub = nullptr;
				break;
			case LUA_TNUMBER:
				sub = lua_tonumber(L, -2);
				break;
			case LUA_TBOOLEAN:
				sub = lua_toboolean(L, -2);
				break;
			case LUA_TSTRING:
				sub = lua_tostring(L, -2);
				break;
			case LUA_TTABLE:
				sub = parse_table(L, -2);
				break;
			default:
				break;
			}

			// set value to key
			if (lua_isnumber(L, -1))
				root.push_back(sub);
			else
				root[lua_tostring(L, -1)] = sub;


			// pop value + copy of key, leaving original key
			lua_pop(L, 2);
			// stack now contains: -1 => key; -2 => table
		}
		// stack now contains: -1 => table (when lua_next returns 0 it pops the key
		// but does not push anything.)
		// Pop table
		lua_pop(L, 1);
		// Stack is now the same as it was on entry to this function

		return root;
	}

	int load_table(lua_State *L, json &j, bool array)
	{
		runtime_state s(L);

		s.create_table();
		{
			if (j.is_array())
				array = true;

			// array iterator (lua arrays start at 1)
			auto i = 1;
			for (auto &kv : j.items())
			{
				if (kv.value().is_string())
					array ? s.set_i(i, kv.value().get<std::string>().c_str()) : s.set_field(kv.key().c_str(), kv.value().get<std::string>().c_str());
				else if (kv.value().is_number())
					array ? s.set_i(i, kv.value().get<double>()) : s.set_field(kv.key().c_str(), kv.value().get<double>());
				else if (kv.value().is_boolean())
					array ? s.set_i(i, kv.value().get<bool>()) : s.set_field(kv.key().c_str(), kv.value().get<bool>());
				else if (kv.value().is_object() || kv.value().is_array())
				{
					load_table(L, kv.value(), kv.value().is_array());
					array ? s.set_i(i) : s.set_field(kv.key().c_str());
				}

				i++;
			}
		}

		return 1;
	}

	void print(const char *info_string)
	{
		con_color_msg(sdk::color(171, 34, 210, 255), XOR("[lua] "));
		con_color_msg(sdk::color(255, 255, 255, 255), info_string);
		con_color_msg(sdk::color(255, 255, 255, 255), XOR("\n"));
	}

	void warn(const char *info_string)
	{
		con_color_msg(sdk::color(171, 34, 210, 255), XOR("[lua] "));
		con_color_msg(sdk::color(255, 128, 0, 255), XOR("warning: "));
		con_color_msg(sdk::color(255, 255, 255, 255), info_string);
		con_color_msg(sdk::color(255, 255, 255, 255), XOR("\n"));
	}

	void error(const char *error_type, const char *error_string)
	{
		con_color_msg(sdk::color(171, 34, 210, 255), XOR("[lua] "));
		con_color_msg(sdk::color(255, 0, 0), error_type);
		con_color_msg(sdk::color(255, 0, 0), XOR(": "));
		con_color_msg(sdk::color(255, 255, 255, 255), error_string);
		con_color_msg(sdk::color(255, 255, 255, 255), XOR("\n"));
		//TODO: EMIT_ERROR_SOUND();
	}

	std::optional<uri> parse_uri(const std::string &u)
	{
		if (u.find(XOR("http")) != 0 || u.size() < 9)
			return std::nullopt;

		uri res{};
		res.is_secure = u.find(XOR("https:")) == 0;

		const auto host_offset = 7 + (int)res.is_secure;
		res.host = u.substr(host_offset, u.find(XOR("/"), 8) - host_offset);
		res.path = u.substr(host_offset + res.host.size());

		if (const auto port_p = res.host.find(XOR(":")); port_p != std::string::npos)
		{
			const auto port_s = res.host.substr(port_p + 1);
			res.host = res.host.substr(0, port_p);
			res.port = std::stoi(port_s);
		}
		else
			res.port = res.is_secure ? 443 : 80;

		if (res.path.empty())
			res.path = XOR("/");

		return res;
	}

	inline std::unordered_map<uint32_t, std::vector<std::shared_ptr<timed_callback>>> timers{};

	timed_callback::timed_callback(float delay, std::function<void()> callback)
	{
		this->delay = delay;
		this->callback = std::move(callback);
	}
}
