#include <regex>

#include "macros.h"
#include "lua/state.h"
#include "lua/api_def.h"
#include "lua/helpers.h"
#include <gui/controls/hotkey.h>

#include "game/cfg.h"
#include "gui/helpers.h"
#include "lua/engine.h"
#include "menu/menu.h"

using namespace evo::gui;
using namespace evo::gui::helpers;

namespace lua::gui_helpers
{
	inline std::tuple<uint64_t, uint64_t, std::shared_ptr<control_container>, bool> get_ids_and_container(runtime_state &s)
	{
		// get our id
		const auto id = hash(s.get_string(1));

		// get container
		const auto cont_id = hash(s.get_string(2));
		auto cont = ctx->find(cont_id);
		if (!cont)
			return {0, 0, nullptr, false};

		bool use_parent{};
		if (cont->type != evo::gui::ctrl_layout && cont->type != evo::gui::ctrl_group)
		{
			cont = cont->parent.lock();
			use_parent = true;
		}

		if (!cont)
			return {0, 0, nullptr, false};

		return std::make_tuple(id, cont_id, cont->as<control_container>(), use_parent);
	}

	inline bool check_eligibility(runtime_state &s, const std::shared_ptr<control_container> &cont, uint64_t id)
	{
		// check if container does exist
		if (!cont)
		{
			s.error(tfm::format(XOR("container was not found: %s"), s.get_string(2)));
			return false;
		}

		// check if element with the same id does already exist
		if (ctx->find(id))
		{
			s.error(tfm::format(XOR("control id is already in use: %s"), s.get_string(1)));
			return false;
		}

		return true;
	}

	template <typename T>
	inline std::optional<int> find_set_bit(const std::string &s, const std::shared_ptr<T> &cb)
	{
		const auto h = ::utils::fnv1a(s.c_str());

		auto n = 0;
		cb->for_each_control_logical(
			[&n, h](std::shared_ptr<control> &c)
			{
				const auto m_s = c->as<selectable>();
				if (::utils::fnv1a(m_s->text.c_str()) == h)
				{
					if (m_s->value != -1)
						n = m_s->value;

					return true;
				}

				++n;
				return false;
			});

		return n >= cb->count() ? std::nullopt : std::make_optional(n);
	}

	inline void mark_tab(const std::shared_ptr<control> &c)
	{
		const auto breadcrumb = c->get_top_breadcrumb().lock();
		if (!breadcrumb || breadcrumb->type != ctrl_tab || breadcrumb->id == GUI_HASH("scripts"))
			return;

		breadcrumb->as<tab>()->is_highlighted = true;
	}
} // namespace lua::gui_helpers

namespace lua::api_def::gui
{
	int color_picker_construct(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::string}, {ltd::string}, {ltd::string}, {ltd::table}, {ltd::boolean, true},});

		if (!r)
		{
			s.error(XOR("usage: color_picker(id, container_id, label, default, allow_alpha = true)"));
			return 0;
		}

		// find our script beforehand
		const auto &me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		// get stuff
		const auto [id, cont_id, cont, up] = gui_helpers::get_ids_and_container(s);

		// check if we can create this control at all
		if (!gui_helpers::check_eligibility(s, cont, id))
			return 0;

		// create the entry in config
		const auto already_exists = cfg.lua_data.c.find(id) != cfg.lua_data.c.end();
		if (!cfg.register_lua_color_entry(id))
		{
			s.error(XOR("config entry ID is already in use internally"));
			return 0;
		}

		if (!already_exists)
			cfg.lua_data.c[id].set(
				{s.get_field_integer(XOR("r"), 4), s.get_field_integer(XOR("g"), 4), s.get_field_integer(XOR("b"), 4), s.get_field_integer(XOR("a"), 4)});

		// finally, create our control
		const auto el = std::make_shared<color_picker>(evo::gui::control_id{id, s.get_string(1)}, cfg.lua_data.c[id], s.get_stack_top() == 5 ? s.get_boolean(5) : true);
		el->do_first_render_call();
		el->reset();

		// add with label to container
		std::shared_ptr<control> c{};
		if (!up)
		{
			c = make_control(s.get_string(1), s.get_string(3), el);
			cont->add(c);
		}
		else
		{
			c = el;
			cont->add(el);
		}

		cont->process_queues();
		cont->reset();

		// register in the script
		gui_helpers::mark_tab(c);
		me->add_gui_element(c->id);

		// create lua usertype
		std::weak_ptr<color_picker> obj{el};
		s.create_user_object<decltype(obj)>(XOR("gui.color_picker"), &obj);

		return 1;
	}

	int textbox_construct(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::string}, {ltd::string},}, true);

		if (!r)
		{
			s.error(XOR("usage: textbox(id, container_id, regex = nil, max_length = nil)"));
			return 0;
		}

		// find our script beforehand
		const auto &me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		// get stuff
		const auto [id, cont_id, cont, up] = gui_helpers::get_ids_and_container(s);
		if (up)
		{
			s.error(XOR("textbox cannot be added to an inline container"));
			return 0;
		}

		// check if we can create this control at all
		if (!gui_helpers::check_eligibility(s, cont, id))
			return 0;

		// finally, create our control
		std::optional<std::regex> regex;
		std::optional<int> max_length;

		if (s.get_stack_top() > 2)
		{
			if (s.is_string(3))
				regex = std::regex(s.get_string(3));
			if (s.get_stack_top() > 3 && s.is_number(4))
				max_length = s.get_integer(4);
		}

		const auto el = std::make_shared<text_input>(
			evo::gui::control_id{id, s.get_string(1)}, evo::ren::vec2{}, evo::ren::vec2{140.f, 24.f}, regex, std::nullopt, max_length);
		el->do_first_render_call();
		el->reset();

		const auto c = make_inlined(hash(std::string(s.get_string(1)) + XOR("_content")), [&](std::shared_ptr<layout> &e) { e->add(el); });
		c->do_first_render_call();
		c->reset();

		cont->add(c);
		cont->process_queues();
		cont->reset();

		// register in the script
		gui_helpers::mark_tab(c);
		me->add_gui_element(c->id);

		// create lua usertype
		std::weak_ptr<text_input> obj{el};
		s.create_user_object<decltype(obj)>(XOR("gui.textbox"), &obj);

		return 1;
	}

	int checkbox_construct(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::string}, {ltd::string}, {ltd::string},});

		if (!r)
		{
			s.error(XOR("usage: checkbox(id, container_id, label)"));
			return 0;
		}

		// find our script beforehand
		const auto &me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		// get stuff
		const auto [id, cont_id, cont, up] = gui_helpers::get_ids_and_container(s);

		// check if we can create this control at all
		if (!gui_helpers::check_eligibility(s, cont, id))
			return 0;

		// create the entry in config
		if (!cfg.register_lua_bool_entry(id))
		{
			s.error(XOR("config entry ID is already in use internally"));
			return 0;
		}

		// finally, create our control
		const auto el = std::make_shared<checkbox>(evo::gui::control_id{id, s.get_string(1)}, cfg.lua_data.b[id]);
		el->do_first_render_call();
		el->reset();

		// add with label to container
		std::shared_ptr<control> c{};
		if (!up)
		{
			c = make_control(s.get_string(1), s.get_string(3), el);
			cont->add(c);
		}
		else
		{
			c = el;
			cont->add(el);
		}

		cont->process_queues();
		cont->reset();

		// register in the script
		gui_helpers::mark_tab(c);
		me->add_gui_element(c->id);

		// create lua usertype
		std::weak_ptr<checkbox> obj{el};
		s.create_user_object<decltype(obj)>(XOR("gui.checkbox"), &obj);

		return 1;
	}

	int slider_construct(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::string}, {ltd::string}, {ltd::string}, {ltd::number}, {ltd::number}, {ltd::string, true}, {ltd::number, true},});

		if (!r)
		{
			s.error(XOR("usage: slider(id, container_id, label, min, max, format = \"%%.0f\")"));
			return 0;
		}

		// find our script beforehand
		const auto &me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		// get stuff
		const auto [id, cont_id, cont, up] = gui_helpers::get_ids_and_container(s);

		// check if we can create this control at all
		if (!gui_helpers::check_eligibility(s, cont, id))
			return 0;

		// create the entry in config
		const auto already_exists = cfg.data.data.find(id) != cfg.data.data.end();
		if (!cfg.register_lua_float_entry(id))
		{
			s.error(XOR("config entry ID is already in use internally"));
			return 0;
		}

		// get format string
		std::string fmt{XOR("%.0f")};
		if (s.get_stack_top() >= 6)
			fmt = s.get_string(6);

		// get step
		float step{1.f};
		if (s.get_stack_top() >= 7)
			step = s.get_float(7);

		// clamp default value
		if (!already_exists)
		{
			if (s.get_float(5) >= 0.f)
				cfg.lua_data.f[id].set(0.f);
			else
				cfg.lua_data.f[id].set(s.get_float(4));
		}

		// finally, create our control
		const auto el = std::make_shared<slider<float>>(
			evo::gui::control_id{id, s.get_string(1)}, s.get_float(4), s.get_float(5), cfg.lua_data.f[id], slider<float>::fmt_step_array{fmt}, step);
		el->do_first_render_call();
		el->reset();

		// add with label to container
		std::shared_ptr<control> c{};
		if (!up)
		{
			c = make_control(s.get_string(1), s.get_string(3), el);
			cont->add(c);
		}
		else
		{
			c = el;
			cont->add(el);
		}

		cont->process_queues();
		cont->reset();

		// register in the script
		gui_helpers::mark_tab(c);
		me->add_gui_element(c->id);

		// create lua usertype
		std::weak_ptr<slider<float>> obj{el};
		s.create_user_object<decltype(obj)>(XOR("gui.slider"), &obj);

		return 1;
	}

	int combobox_construct(lua_State *l)
	{
		runtime_state s(l);

		auto r = s.check_arguments({{ltd::string}, {ltd::string}, {ltd::string}, {ltd::boolean}, {ltd::string},}, true);

		if (!r)
		{
			r = s.check_arguments({{ltd::string}, {ltd::string}, {ltd::string}, {ltd::string},}, true);

			if (!r)
			{
				s.error(XOR("usage: combobox(id, container_id, label, multi_select, values...)"));
				return 0;
			}

			lua::helpers::warn(
				XOR("deprecated: combobox(id, container_id, label, values...)\nuse combobox(id, " "container_id, label, multi_select (true/false), values...) instead"));
		}

		// find our script beforehand
		const auto &me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		// get stuff
		const auto [id, cont_id, cont, up] = gui_helpers::get_ids_and_container(s);

		// check if we can create this control at all
		if (!gui_helpers::check_eligibility(s, cont, id))
			return 0;

		// create the entry in config
		if (!cfg.register_lua_bit_entry(id))
		{
			s.error(XOR("config entry ID is already in use internally"));
			return 0;
		}

		// finally, create our control
		const auto el = std::make_shared<combo_box>(evo::gui::control_id{id, s.get_string(1)}, cfg.lua_data.s[id]);

		// make multi_select (optional)
		if (s.is_boolean(4) && s.get_boolean(4))
			el->allow_multiple = true;

		for (auto i = 4; i <= s.get_stack_top(); ++i)
		{
			if (!s.check_argument_type(i, ltd::string))
				continue;

			const auto sel = std::make_shared<selectable>(evo::gui::control_id{hash(s.get_string(1) + std::string(XOR(".")) + s.get_string(i))}, s.get_string(i));
			el->add(sel);
		}

		el->process_queues();
		el->do_first_render_call();
		el->reset();

		// add with label to container
		std::shared_ptr<control> c{};
		if (!up)
		{
			c = make_control(s.get_string(1), s.get_string(3), el);
			cont->add(c);
		}
		else
		{
			c = el;
			cont->add(el);
		}

		cont->process_queues();
		cont->reset();

		// register in the script
		gui_helpers::mark_tab(c);
		me->add_gui_element(c->id);

		// create lua usertype
		std::weak_ptr<combo_box> obj{el};
		s.create_user_object<decltype(obj)>(XOR("gui.combobox"), &obj);

		return 1;
	}

	int label_construct(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::string}, {ltd::string}, {ltd::string},});

		if (!r)
		{
			s.error(XOR("usage: label(id, container_id, text)"));
			return 0;
		}

		// find our script beforehand
		const auto &me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		// get stuff
		const auto [id, cont_id, cont, up] = gui_helpers::get_ids_and_container(s);
		if (up)
		{
			s.error(XOR("label cannot be added to an inline container"));
			return 0;
		}

		// check if we can create this control at all
		if (!gui_helpers::check_eligibility(s, cont, id))
			return 0;

		// finally, create our control
		const auto el = std::make_shared<label>(evo::gui::control_id{id, s.get_string(1)}, s.get_string(3));
		el->do_first_render_call();
		el->reset();

		cont->add(el);
		cont->process_queues();
		cont->reset();

		// register in the script
		gui_helpers::mark_tab(el);
		me->add_gui_element(el->id);

		// create lua usertype
		std::weak_ptr<label> obj{el};
		s.create_user_object<decltype(obj)>(XOR("gui.label"), &obj);

		return 1;
	}

	int button_construct(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::string}, {ltd::string}, {ltd::string},});

		if (!r)
		{
			s.error(XOR("usage: button(id, container_id, text)"));
			return 0;
		}

		// find our script beforehand
		const auto &me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		// get stuff
		const auto [id, cont_id, cont, up] = gui_helpers::get_ids_and_container(s);
		if (up)
		{
			s.error(XOR("button cannot be added to an inline container"));
			return 0;
		}

		// check if we can create this control at all
		if (!gui_helpers::check_eligibility(s, cont, id))
			return 0;

		// finally, create our control
		const auto el = std::make_shared<button>(evo::gui::control_id{id, s.get_string(1)}, s.get_string(3));
		el->size.x = 160.f;
		el->do_first_render_call();
		el->reset();

		cont->add(el);
		cont->process_queues();
		cont->reset();

		// register in the script
		gui_helpers::mark_tab(el);
		me->add_gui_element(el->id);

		// create lua usertype
		std::weak_ptr<button> obj{el};
		s.create_user_object<decltype(obj)>(XOR("gui.button"), &obj);

		return 1;
	}

	int color_picker_get_value(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::user_data},});

		if (!r)
		{
			s.error(XOR("usage: obj:get_value()"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<color_picker> *>(s.get_user_data(1));
		if (const auto el = obj.lock(); el)
		{
			s.create_table();
			s.set_field(XOR("r"), el->value->value.r * 255.f);
			s.set_field(XOR("g"), el->value->value.g * 255.f);
			s.set_field(XOR("b"), el->value->value.b * 255.f);
			s.set_field(XOR("a"), el->value->value.a * 255.f);
			return 1;
		}

		s.error(XOR("control does not exist"));
		return 0;
	}

	int color_picker_set_value(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::user_data}, {ltd::table}});

		if (!r)
		{
			s.error(XOR("usage: obj:set_value(v)"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<color_picker> *>(s.get_user_data(1));
		if (const auto el = obj.lock(); el)
		{
			el->value.set({s.get_field_integer(XOR("r"), 2), s.get_field_integer(XOR("g"), 2), s.get_field_integer(XOR("b"), 2), s.get_field_integer(XOR("a"), 2)});

			el->reset();

			return 0;
		}

		s.error(XOR("control does not exist"));
		return 0;
	}

	int checkbox_get_value(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::user_data},});

		if (!r)
		{
			s.error(XOR("usage: obj:get_value()"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<checkbox> *>(s.get_user_data(1));
		if (const auto el = obj.lock(); el)
		{
			s.push(el->value.get());
			return 1;
		}

		s.error(XOR("control does not exist"));
		return 0;
	}

	int checkbox_set_value(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::user_data}, {ltd::boolean}});

		if (!r)
		{
			s.error(XOR("usage: obj:set_value(v)"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<checkbox> *>(s.get_user_data(1));
		if (const auto el = obj.lock(); el)
		{
			el->value.set(s.get_boolean(2));
			el->reset();

			return 0;
		}

		s.error(XOR("control does not exist"));
		return 0;
	}

	int textbox_get_value(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::user_data},});

		if (!r)
		{
			s.error(XOR("usage: obj:get_value()"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<text_input> *>(s.get_user_data(1));
		if (const auto el = obj.lock(); el)
		{
			s.push(el->value.c_str());
			return 1;
		}

		s.error(XOR("control does not exist"));
		return 0;
	}

	int textbox_set_value(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::user_data}, {ltd::string}});

		if (!r)
		{
			s.error(XOR("usage: obj:set_value(v)"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<text_input> *>(s.get_user_data(1));
		if (const auto el = obj.lock(); el)
		{
			el->set_value(s.get_string(2));
			el->reset();

			return 0;
		}

		s.error(XOR("control does not exist"));
		return 0;
	}

	int combobox_get_value(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::user_data}}, true);
		if (!r)
		{
			s.error(XOR("usage: obj:get_value(...)"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<combo_box> *>(s.get_user_data(1));
		if (const auto el = obj.lock(); el)
		{
			const auto set = el->value->first_set_bit();
			if (el->allow_multiple)
			{
				const auto as_table = s.get_stack_top() == 2 && s.is_boolean(2) && s.get_boolean(2);
				const auto as_bool = s.get_stack_top() == 2 && s.is_string(2);

				if (as_table)
					s.create_table();

				if (!set)
				{
					if (as_bool)
					{
						s.push(false);
						return 1;
					}

					return as_table ? 1 : 0;
				}

				auto n = 0;
				auto added = 0;
				bool found_selected{};
				uint32_t val_hash{};

				if (as_bool)
					val_hash = ::utils::fnv1a(s.get_string(2));

				el->for_each_control(
					[&](std::shared_ptr<control> &m)
					{
						const auto m_s = m->as<selectable>();
						if (!m_s || as_bool && found_selected)
							return;

						const auto v = m_s->value != -1 ? m_s->value : n;
						if (el->value->test((char)v))
						{
							if (as_bool)
							{
								found_selected = val_hash == ::utils::fnv1a(m_s->text.c_str());
								++n;
								return;
							}

							if (!as_table)
								s.push(m_s->text.c_str());
							else
								s.set_i(added + 1, m_s->text.c_str());

							++added;
						}

						++n;
					});

				if (as_bool)
				{
					s.push(found_selected);
					return 1;
				}

				return as_table ? 1 : added;
			}
			else
			{
				auto n = 0;
				el->for_each_control_logical(
					[&](std::shared_ptr<control> &m)
					{
						const auto m_s = m->as<selectable>();
						if (!m_s)
							return false;

						const auto v = m_s->value != -1 ? m_s->value : n;

						if ((el->legacy_mode && static_cast<int>(el->value.get()) == m_s->value) || (!el->legacy_mode && set == v))
						{
							s.push(m_s->text.c_str());
							return true;
						}

						n++;
						return false;
					});

				return 1;
			}
		}

		s.error(XOR("control does not exist"));
		return 0;
	}

	int combobox_set_value(lua_State *l)
	{
		runtime_state s(l);
		if (s.get_stack_top() < 2 || !s.check_argument_type(1, ltd::user_data))
		{
			s.error(XOR("usage: obj:set_value(values...)"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<combo_box> *>(s.get_user_data(1));
		if (const auto el = obj.lock(); el)
		{
			el->value.get().reset();
			if (!el->allow_multiple)
			{
				if (!s.check_argument_type(2, ltd::string))
				{
					s.error(XOR("usage: obj:set_value(value)"));
					return 0;
				}

				const auto n = gui_helpers::find_set_bit(s.get_string(2), el);
				if (!n.has_value())
				{
					s.error(XOR("entry not found"));
					return 0;
				}

				el->legacy_mode ? el->value.set(*n) : el->value.get().set(static_cast<char>(*n));
			}
			else
			{
				std::vector<std::string> vals;
				if (s.check_argument_type(2, ltd::table))
					vals = s.table_to_string_array(2);
				else
				{
					for (auto i = 2; i <= s.get_stack_top(); ++i)
					{
						if (!s.check_argument_type(i, ltd::string))
							continue;

						vals.emplace_back(s.get_string(i));
					}
				}

				if (!vals.empty())
				{
					for (const auto &v : vals)
					{
						const auto n = gui_helpers::find_set_bit(v, el);
						if (!n.has_value())
							continue;

						el->value.get().set(static_cast<char>(*n));
					}
				}
				else
					el->value.set(0);
			}

			el->reset();
		}

		return 0;
	}

	int combobox_get_options(lua_State *l)
	{
		runtime_state s(l);
		if (!s.check_arguments({{ltd::user_data}}))
		{
			s.error(XOR("usage: obj:get_options()"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<combo_box> *>(s.get_user_data(1));
		if (obj.expired())
			return 0;

		const auto el = obj.lock();
		s.create_table();

		int i{};
		el->for_each_control(
			[&](auto &c)
			{
				if (!c)
					return;
				s.set_i(++i, c->template as<selectable>()->text);
			});

		return 1;
	}

	int slider_get_value(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::user_data},});

		if (!r)
		{
			s.error(XOR("usage: obj:get_value()"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<slider<float>> *>(s.get_user_data(1));
		if (const auto el = obj.lock(); el)
		{
			s.push(el->value.get());
			return 1;
		}

		s.error(XOR("control does not exist"));
		return 0;
	}

	int slider_set_value(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::user_data}, {ltd::number}});

		if (!r)
		{
			s.error(XOR("usage: obj:set_value(v)"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<slider<float>> *>(s.get_user_data(1));
		if (const auto el = obj.lock(); el)
		{
			el->value.set(std::clamp(s.get_float(2), el->low, el->high));
			el->reset();

			return 0;
		}

		s.error(XOR("control does not exist"));
		return 0;
	}

	int control_set_tooltip(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::user_data}, {ltd::string}});

		if (!r)
		{
			s.error(XOR("usage: obj:set_tooltip(text)"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<control> *>(s.get_user_data(1));
		if (const auto el = obj.lock(); el)
		{
			el->tooltip = s.get_string(2);
			return 0;
		}

		s.error(XOR("control does not exist"));
		return 0;
	}

	int control_set_visible(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::user_data}, {ltd::boolean}});

		if (!r)
		{
			s.error(XOR("usage: obj:set_visible(v)"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<control> *>(s.get_user_data(1));
		if (const auto el = obj.lock(); el)
		{
			const auto p = el->parent.lock();
			if (!p || !p->is_container)
			{
				s.error(XOR("broken control"));
				return 0;
			}

			if (p->as<layout>()->direction == s_inline)
				p->set_visible(s.get_boolean(2));
			else
				el->set_visible(s.get_boolean(2));

			return 0;
		}

		s.error(XOR("control does not exist"));
		return 0;
	}

	int control_add_callback(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::user_data}, {ltd::function}});

		if (!r)
		{
			s.error(XOR("usage: obj:add_callback(callback())"));
			return 0;
		}

		const auto &me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<control> *>(s.get_user_data(1));
		if (const auto el = obj.lock(); el)
		{
			const auto ref = s.registry_add();
			el->universal_callbacks[me->id].emplace_back(
				[ref, l, me]()
				{
					runtime_state s(l);
					s.registry_get(ref);

					if (!s.is_nil(-1))
					{
						if (!s.call(0, 0))
						{
							me->did_error = true;
							lua::helpers::error(XOR("runtime_error"), s.get_last_error_description().c_str());
							if (const auto f = api.find_script_file(me->id); f.has_value())
								f->get().should_unload = true;

							return;
						}
					}
					else
						s.pop(1);
				});

			me->add_gui_element_with_callback(el->id);
		}

		return 0;
	}

	int get_checkbox(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::string},});

		if (!r)
		{
			s.error(XOR("usage: get_checkbox(id)"));
			return 0;
		}

		const auto el = ctx->find<checkbox>(hash(s.get_string(1)));
		if (!el)
			return 0;

		std::weak_ptr<checkbox> obj{el};
		s.create_user_object<decltype(obj)>(XOR("gui.checkbox"), &obj);

		return 1;
	}

	int get_color_picker(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::string},});

		if (!r)
		{
			s.error(XOR("usage: get_color_picker(id)"));
			return 0;
		}

		const auto el = ctx->find<color_picker>(hash(s.get_string(1)));
		if (!el)
			return 0;

		std::weak_ptr<color_picker> obj{el};
		s.create_user_object<decltype(obj)>(XOR("gui.color_picker"), &obj);

		return 1;
	}

	int get_slider(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::string},});

		if (!r)
		{
			s.error(XOR("usage: get_slider(id)"));
			return 0;
		}

		const auto el = ctx->find<slider<float>>(hash(s.get_string(1)));
		if (!el)
			return 0;

		std::weak_ptr<slider<float>> obj{el};
		s.create_user_object<decltype(obj)>(XOR("gui.slider"), &obj);

		return 1;
	}

	int get_combobox(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::string},});

		if (!r)
		{
			s.error(XOR("usage: get_combobox(id)"));
			return 0;
		}

		const auto el = ctx->find<combo_box>(hash(s.get_string(1)));
		if (!el)
			return 0;

		std::weak_ptr<combo_box> obj{el};
		s.create_user_object<decltype(obj)>(XOR("gui.combobox"), &obj);

		return 1;
	}


	int get_list(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::string},});

		if (!r)
		{
			s.error(XOR("usage: get_list(id)"));
			return 0;
		}

		const auto el = ctx->find<list>(hash(s.get_string(1)));
		if (!el)
			return 0;

		std::weak_ptr<list> obj{el};
		s.create_user_object<decltype( obj )>(XOR("gui.list"), &obj);

		return 1;
	}

	int add_notification(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::string}, {ltd::string},});

		if (!r)
		{
			s.error(XOR("usage: add_notification(header, body)"));
			return 0;
		}

		notify.add(std::make_shared<notification>(s.get_string(1), s.get_string(2)));
		return 0;
	}

	int for_each_hotkey(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::function},});

		if (!r)
		{
			s.error(XOR("usage: for_each_hotkey(callback(name, key, mode, is_active))"));
			return 0;
		}

		const auto me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		const auto ref = s.registry_add();
		for (const auto &[id, hk_weak] : ctx->hotkey_list)
		{
			const auto hk = hk_weak.lock();
			if (!hk)
				continue;

			if (hk->hotkey_type == hkt_none || hk->hotkeys.empty())
				continue;

			const auto p = hk->parent.lock();
			if (!p || !p->is_container)
				continue;

			const auto possible_label = p->as<control_container>()->at(0);
			if (possible_label->type != ctrl_label)
				continue;

			for (const auto &[hk_v, hk_i] : hk->hotkeys)
			{
				// skip if it was unbound
				if (!hk_v)
					continue;

				s.registry_get(ref);
				s.push(possible_label->as<label>()->text.c_str());
				s.push(key_to_string(hk_v).c_str());
				s.push(static_cast<int>(hk_i.beh));

				if (hk->type == ctrl_checkbox)
					s.push(hk->as<checkbox>()->value.get());
				else if (hk->type == ctrl_slider)
				{
					const auto sld = hk->as<slider<float>>();
					if (sld->hk_type == st_float)
						s.push(sld->value.get());
					else
						s.push(hk->as<slider<int>>()->value.get());
				}
				else if (hk->type == ctrl_combo_box)
				{
					const auto cb = hk->as<combo_box>();
					bool as_table{};
					if (cb->allow_multiple)
					{
						s.create_table();
						as_table = true;
					}

					int added{}, n{};
					cb->for_each_control(
						[&](std::shared_ptr<control> &m)
						{
							const auto m_s = m->as<selectable>();
							if (!m_s)
								return;

							const auto v = m_s->value != -1 ? m_s->value : n;
							if (cb->value->test((char)v))
							{
								if (!as_table)
									s.push(m_s->text.c_str());
								else
									s.set_i(added + 1, m_s->text.c_str());

								++added;
							}

							++n;
						});
				}

				if (!s.call(4, 0))
				{
					me->did_error = true;
					lua::helpers::error(XOR("runtime_error"), s.get_last_error_description().c_str());
					if (const auto f = api.find_script_file(me->id); f.has_value())
						f->get().should_unload = true;

					return 0;
				}
			}
		}

		s.registry_remove(ref);
		return 0;
	}

	int is_menu_open(lua_State *l)
	{
		runtime_state s(l);
		s.push(menu::men.is_open());

		return 1;
	}

	int show_message(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::string}, {ltd::string}, {ltd::string},});

		if (!r)
		{
			s.error(XOR("usage: show_message(id, header, body)"));
			return 0;
		}

		const auto msg = std::make_shared<message_box>(evo::gui::control_id{evo::gui::hash(s.get_string(1)), s.get_string(1)}, s.get_string(2), s.get_string(3));
		msg->open();

		return 0;
	}

	int show_dialog(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::string}, {ltd::string}, {ltd::string}, {ltd::number}, {ltd::function},});

		if (!r)
		{
			s.error(XOR("usage: show_dialog(id, header, body, buttons, callback)"));
			return 0;
		}

		const auto me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		// save up function in the registry
		const auto ref = s.registry_add();

		const auto dlg = std::make_shared<dialog_box>(
			evo::gui::control_id{evo::gui::hash(s.get_string(1)), s.get_string(1)}, s.get_string(2), s.get_string(3), (dialog_buttons)s.get_integer(4));
		dlg->callback = [ref, l, me](dialog_result r)
		{
			runtime_state s(l);
			s.registry_get(ref);
			s.push((int)r);
			if (!s.call(1, 0))
			{
				me->did_error = true;
				lua::helpers::error(XOR("runtime_error"), s.get_last_error_description().c_str());
				if (const auto f = api.find_script_file(me->id); f.has_value())
					f->get().should_unload = true;

				return;
			}
			s.registry_remove(ref);
		};

		dlg->open();

		return 0;
	}

	int get_menu_rect(lua_State *l)
	{
		runtime_state s(l);

		const auto wnd = menu::men.main_wnd.lock();
		if (!wnd)
			return 0;

		const auto r = wnd->area_abs();
		s.push(r.mins.x);
		s.push(r.mins.y);
		s.push(r.maxs.x);
		s.push(r.maxs.y);

		return 4;
	}

	int list_construct(lua_State *l)
	{
		runtime_state s(l);
		if (!s.check_arguments({{ltd::string}, {ltd::string}, {ltd::boolean}, {ltd::number, true}}))
		{
			s.error(XOR("usage: gui.list(id, container_id, is_multi, height)"));
			return 0;
		}

		// find our script beforehand
		const auto &me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		// get stuff
		const auto [id, cont_id, cont, up] = gui_helpers::get_ids_and_container(s);

		// check if we can create this control at all
		if (!gui_helpers::check_eligibility(s, cont, id))
			return 0;

		// create the entry in config
		if (!cfg.register_lua_bit_entry(id))
		{
			s.error(XOR("config entry ID is already in use internally"));
			return 0;
		}

		const auto height = s.get_stack_top() >= 4 ? s.get_float(4) : 120.f;
		const auto el = std::make_shared<list>(
			evo::gui::control_id{id, s.get_string(1)}, cfg.lua_data.s[id], evo::ren::vec2{}, evo::ren::vec2{140.f, std::max(height, 40.f)});
		el->margin.mins.x = 2.f;
		el->allow_multiple = s.get_boolean(3);
		el->legacy_mode = !el->allow_multiple;
		el->do_first_render_call();
		el->reset();

		const auto c = make_inlined(hash(std::string(s.get_string(1)) + XOR("_content")), [&](std::shared_ptr<layout> &e) { e->add(el); });
		c->do_first_render_call();
		c->reset();

		cont->add(c);
		cont->process_queues();
		cont->reset();

		// register in the script
		gui_helpers::mark_tab(c);
		me->add_gui_element(c->id);

		// create lua usertype
		std::weak_ptr<list> obj{el};
		s.create_user_object<decltype(obj)>(XOR("gui.list"), &obj);

		return 1;
	}

	int list_get_value(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::user_data}}, true);
		if (!r)
		{
			s.error(XOR("usage: obj:get_value(...)"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<list> *>(s.get_user_data(1));
		if (const auto el = obj.lock(); el)
		{
			const auto set = el->value->first_set_bit();
			if (el->allow_multiple)
			{
				const auto as_table = s.get_stack_top() == 2 && s.is_boolean(2) && s.get_boolean(2);
				const auto as_bool = s.get_stack_top() == 2 && s.is_string(2);

				if (as_table)
					s.create_table();

				if (!set)
				{
					if (as_bool)
					{
						s.push(false);
						return 1;
					}

					return as_table ? 1 : 0;
				}

				auto n = 0;
				auto added = 0;
				bool found_selected{};
				uint32_t val_hash{};

				if (as_bool)
					val_hash = ::utils::fnv1a(s.get_string(2));

				el->for_each_control(
					[&](std::shared_ptr<control> &m)
					{
						const auto m_s = m->as<selectable>();
						if (!m_s || as_bool && found_selected)
							return;

						const auto v = m_s->value != -1 ? m_s->value : n;
						if (el->value->test((char)v))
						{
							if (as_bool)
							{
								found_selected = val_hash == ::utils::fnv1a(m_s->text.c_str());
								++n;
								return;
							}

							if (!as_table)
								s.push(m_s->text.c_str());
							else
								s.set_i(added + 1, m_s->text.c_str());

							++added;
						}

						++n;
					});

				if (as_bool)
				{
					s.push(found_selected);
					return 1;
				}

				return as_table ? 1 : added;
			}
			else
			{
				auto n = 0;
				el->for_each_control_logical(
					[&](std::shared_ptr<control> &m)
					{
						const auto m_s = m->as<selectable>();
						if (!m_s)
							return false;

						const auto v = m_s->value != -1 ? m_s->value : n;

						if ((el->legacy_mode && static_cast<int>(el->value.get()) == m_s->value) || (!el->legacy_mode && set == v))
						{
							s.push(m_s->text.c_str());
							return true;
						}

						n++;
						return false;
					});

				return 1;
			}
		}

		s.error(XOR("control does not exist"));
		return 0;
	}

	int list_set_value(lua_State *l)
	{
		runtime_state s(l);
		if (s.get_stack_top() < 2 || !s.check_argument_type(1, ltd::user_data))
		{
			s.error(XOR("usage: obj:set_value(values...)"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<list> *>(s.get_user_data(1));
		if (const auto el = obj.lock(); el)
		{
			el->value.get().reset();
			if (!el->allow_multiple)
			{
				const auto n = gui_helpers::find_set_bit(s.get_string(2), el);
				if (!n.has_value())
				{
					s.error(XOR("entry not found"));
					return 0;
				}

				el->legacy_mode ? el->value.set(*n) : el->value.get().set(static_cast<char>(*n));
			}
			else
			{
				std::vector<std::string> vals;
				if (s.check_argument_type(2, ltd::table))
					vals = s.table_to_string_array(2);
				else
				{
					for (auto i = 2; i <= s.get_stack_top(); ++i)
					{
						if (!s.check_argument_type(i, ltd::string))
							continue;

						vals.emplace_back(s.get_string(i));
					}
				}

				if (!vals.empty())
				{
					for (const auto &v : vals)
					{
						const auto n = gui_helpers::find_set_bit(v, el);
						if (!n.has_value())
							continue;

						el->value.get().set(static_cast<char>(*n));
					}
				}
				else
					el->value.set(0);
			}

			el->reset();
		}

		return 0;
	}

	int list_add(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::user_data}, {ltd::string}});
		if (!r)
		{
			s.error(XOR("usage: obj:add(option)"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<list> *>(s.get_user_data(1));
		if (obj.expired())
			return 0;

		const auto el = obj.lock();
		const auto option_id = hash(el->id_string + XOR(".") + s.get_string(2));
		if (el->find(option_id))
		{
			s.error(XOR("provided option already exists"));
			return 0;
		}

		if (el->allow_multiple && el->count() >= 64)
		{
			s.error(XOR("this list can hold only up to 64 elements"));
			return 0;
		}
		else if (el->legacy_mode && el->count() >= 128)
		{
			s.error(XOR("this list can hold only up to 128 elements"));
			return 0;
		}

		const auto was_empty = !el->count();
		el->add(std::make_shared<selectable>(control_id{option_id}, s.get_string(2), el->count()));
		el->process_queues();

		if ((was_empty || el->value.get() < 0 || el->value.get() >= el->count()) && el->legacy_mode)
		{
			el->value.set(0);
			el->reset();
		}

		return 0;
	}

	int list_remove(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::user_data}, {ltd::string}});
		if (!r)
		{
			s.error(XOR("usage: obj:remove(option)"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<list> *>(s.get_user_data(1));
		if (obj.expired())
			return 0;

		const auto el = obj.lock();
		el->remove(hash(el->id_string + XOR(".") + s.get_string(2)));
		el->process_queues();

		if (el->value.get() >= el->count() && el->legacy_mode)
		{
			el->value.set(0);
			el->reset();
		}

		return 0;
	}

	int list_get_options(lua_State *l)
	{
		runtime_state s(l);
		if (!s.check_arguments({{ltd::user_data}}))
		{
			s.error(XOR("usage: obj:get_options()"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<list> *>(s.get_user_data(1));
		if (obj.expired())
			return 0;

		const auto el = obj.lock();
		s.create_table();

		int i{};
		el->for_each_control(
			[&](auto &c)
			{
				if (!c)
					return;
				s.set_i(++i, c->template as<selectable>()->text);
			});

		return 1;
	}

	int control_get_name(lua_State *l)
	{
		runtime_state s(l);
		if (!s.check_arguments({{ltd::user_data}}))
		{
			s.error(XOR("usage: obj:get_name()"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<control> *>(s.get_user_data(1));
		if (obj.expired())
			return 0;

		const auto el = obj.lock();
		if (el->parent.expired())
			return 0;

		const auto par = el->parent.lock();
		if (!par->is_container)
			return 0;

		std::string name{};
		par->as<control_container>()->for_each_control_logical(
			[&](auto &c)
			{
				if (!c || c->type != ctrl_label)
					return false;
				name = c->template as<label>()->text;
				return true;
			});

		s.push(name);
		return 1;
	}

	int control_get_type(lua_State *l)
	{
		runtime_state s(l);
		if (!s.check_arguments({{ltd::user_data}}))
		{
			s.error(XOR("usage: obj:get_type()"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<control> *>(s.get_user_data(1));
		if (obj.expired())
			return 0;

		switch (obj.lock()->type)
		{
		default:
			s.push(XOR("unknown"));
			break;
		case ctrl_button:
			s.push(XOR("button"));
			break;
		case ctrl_checkbox:
			s.push(XOR("checkbox"));
			break;
		case ctrl_color_picker:
			s.push(XOR("color_picker"));
			break;
		case ctrl_combo_box:
			s.push(XOR("combobox"));
			break;
		case ctrl_label:
			s.push(XOR("label"));
			break;
		case ctrl_list:
			s.push(XOR("list"));
			break;
		case ctrl_slider:
			s.push(XOR("slider"));
			break;
		case ctrl_text_input:
			s.push(XOR("textbox"));
			break;
		}

		return 1;
	}
} // namespace lua::api_def::gui
