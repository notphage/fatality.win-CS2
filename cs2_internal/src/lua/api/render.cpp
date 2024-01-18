#include "macros.h"
#include "lua/state.h"
#include "lua/api_def.h"
#include "lua/helpers.h"
#include <ren/adapter.h>
#include <game/draw_manager.h>
#include <ren/types/font.h>
#include <ren/types/animated_texture.h>

#include "gui/gui.h"
#include "gui/misc.h"
#include "lua/engine.h"

using namespace evo;
using namespace lua;

namespace lua::api_def::render
{
	int create_font(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({
			{ltd::string},
			{ltd::number},
			{ltd::number, true},
			{ltd::number, true},
			{ltd::number, true},
		});

		if (!r)
		{
			s.error(XOR("usage: create_font(font_path, size, flags = none, from = 0, to = 255): number"));
			return 0;
		}

		const auto me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		if (s.get_integer(2) <= 0)
		{
			s.error(XOR("font size must be greater than 0"));
			return 0;
		}

		const auto flags = s.get_stack_top() >= 3 ? s.get_integer(3) : 0;
		const auto from = s.get_stack_top() >= 4 ? s.get_integer(4) : 0;
		const auto to = s.get_stack_top() >= 5 ? s.get_integer(5) : 255;

		const auto next_free_id = api.font.get_empty_slot();
		const auto &fnt = ren::draw.fonts[next_free_id] =
			std::make_shared<ren::font>(s.get_string(1), s.get_integer(2), flags | ren::font_flag_no_dpi, from, to);
		api.font.on_take(next_free_id);

		me->add_font(next_free_id);
		fnt->create();

		s.push(next_free_id);
		return 1;
	}

	int create_font_stream(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({
			{ltd::table},
			{ltd::number},
			{ltd::number, true},
			{ltd::number, true},
			{ltd::number, true},
		});

		if (!r)
		{
			s.error(XOR("usage: create_font(bytes_table, size, flags = none, from = 0, to = 255): number"));
			return 0;
		}

		const auto me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		if (s.get_integer(2) <= 0)
		{
			s.error(XOR("font size must be greater than 0"));
			return 0;
		}

		std::vector<uint8_t> bytes;
		for (const auto &b : s.table_to_int_array(1))
		{
			if ((uint32_t)b > 255)
			{
				s.error(XOR("invalid byte value"));
				return 0;
			}

			bytes.push_back(static_cast<uint8_t>(b));
		}

		const auto flags = s.get_stack_top() >= 3 ? s.get_integer(3) : 0;
		const auto from = s.get_stack_top() >= 4 ? s.get_integer(4) : 0;
		const auto to = s.get_stack_top() >= 5 ? s.get_integer(5) : 255;

		const auto next_free_id = api.font.get_empty_slot();
		const auto &fnt = ren::draw.fonts[next_free_id] =
			std::make_shared<ren::font>(bytes.data(), bytes.size(), s.get_integer(2), flags | ren::font_flag_no_dpi,
										from, to);
		me->add_font(next_free_id);
		api.font.on_take(next_free_id);
		fnt->create();

		s.push(next_free_id);
		return 1;
	}

	int create_font_gdi(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({
			{ltd::string},
			{ltd::number},
			{ltd::number, true},
			{ltd::number, true},
			{ltd::number, true},
		});

		if (!r)
		{
			s.error(XOR("usage: create_font_gdi(name, size, flags = none, from = 0, to = 255): number"));
			return 0;
		}

		const auto me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		const auto flags = s.get_stack_top() >= 3 ? s.get_integer(3) : 0;
		const auto from = s.get_stack_top() >= 4 ? s.get_integer(4) : 0;
		const auto to = s.get_stack_top() >= 5 ? s.get_integer(5) : 255;

		const auto next_free_id = api.font.get_empty_slot();
		const auto &fnt = ren::draw.fonts[next_free_id] =
			std::make_shared<ren::font_gdi>(s.get_string(1), s.get_integer(2), flags | ren::font_flag_no_dpi, from, to);
		me->add_font(next_free_id);
		api.font.on_take(next_free_id);
		fnt->create();

		s.push(next_free_id);
		return 1;
	}

	int create_texture(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({
			{ltd::string},
		});

		if (!r)
		{
			s.error(XOR("usage: create_texture(tex_path): number"));
			return 0;
		}

		const auto me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		const auto next_free_id = api.texture.get_empty_slot();
		const auto &tex = ren::draw.textures[next_free_id] = std::make_shared<ren::animated_texture>(s.get_string(1));
		me->add_texture(next_free_id);
		api.texture.on_take(next_free_id);
		tex->create();

		s.push(next_free_id);
		return 1;
	}

	int create_texture_stream(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({
			{ltd::table},
		});

		if (!r)
		{
			s.error(XOR("usage: create_texture_stream(bytes_table): number"));
			return 0;
		}

		const auto me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		std::vector<uint8_t> bytes;
		for (const auto &b : s.table_to_int_array(1))
		{
			if ((uint32_t)b > 255)
			{
				s.error(XOR("invalid byte value"));
				return 0;
			}

			bytes.push_back(static_cast<uint8_t>(b));
		}

		const auto next_free_id = api.texture.get_empty_slot();
		const auto &tex = ren::draw.textures[next_free_id] =
			std::make_shared<ren::animated_texture>(bytes.data(), bytes.size());
		me->add_texture(next_free_id);
		api.texture.on_take(next_free_id);
		tex->create();

		s.push(next_free_id);
		return 1;
	}

	int create_texture_bytes(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({
			{ltd::number},
			{ltd::number},
		});

		if (!r)
		{
			s.error(XOR("usage: create_texture_bytes(bytes, size): number"));
			return 0;
		}

		const auto me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		const auto next_free_id = api.texture.get_empty_slot();
		const auto &tex = ren::draw.textures[next_free_id] =
			std::make_shared<ren::animated_texture>((void *)s.get_integer(1), s.get_integer(2));
		me->add_texture(next_free_id);
		api.texture.on_take(next_free_id);
		tex->create();

		s.push(next_free_id);
		return 1;
	}

	int create_texture_rgba(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::number},
		});

		if (!r)
		{
			s.error(XOR("usage: create_texture_rgba(bytes, w, h, stride): number"));
			return 0;
		}

		const auto me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		const auto next_free_id = api.texture.get_empty_slot();
		const auto &tex = ren::draw.textures[next_free_id] =
			std::make_shared<ren::texture>((void *)s.get_integer(1), s.get_integer(2), s.get_integer(3),
										   s.get_integer(4));
		me->add_texture(next_free_id);
		api.texture.on_take(next_free_id);
		tex->create();

		s.push(next_free_id);
		return 1;
	}

	//int create_texture_svg(lua_State* l)
	//{
	//	runtime_state s(l);
	//	const auto r = s.check_arguments({ {ltd::string}, {ltd::number} });

	//	if (!r)
	//	{
	//		s.error(XOR("usage: create_texture_svg(svg, height): number"));
	//		return 0;
	//	}

	//	const auto me = api.find_by_state(l);
	//	if (!me)
	//	{
	//		s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
	//		return 0;
	//	}

	//	std::string file{ s.get_string(1) };
	//	if (file.size() < 4)
	//	{
	//		s.error(XOR("invalid SVG file"));
	//		return 0;
	//	}

	//	if (file.front() == ' ')
	//		file.erase(0, file.find_first_not_of(' '));
	//	if (file.back() == ' ')
	//		file.erase(file.find_last_not_of(' ') + 1);

	//	// append header in case it's missing
	//	if (!file.starts_with(XOR("<?xml")))
	//		file = XOR("<?xml version=\"1.0\" encoding=\"utf-8\"?>") + file;

	//	// load in-place to get dimension.
	//	std::vector<uint32_t> texture(0xFFFFFF);
	//	image_data img(texture);
	//	uint32_t w{}, h{};
	//	if (!img.load_svg((uint8_t*)file.c_str(), file.size(), &w, &h))
	//		return 0;

	//	if (w == 0 || h == 0)
	//		return 0;

	//	const auto width_to_height = static_cast<float>(w) / static_cast<float>(h);
	//	const auto next_free_id = api.texture.get_empty_slot();

	//	w = uint32_t(width_to_height * s.get_float(2));
	//	h = uint32_t(s.get_float(2));

	//	texture.clear();
	//	if (!img.load_svg((uint8_t*)file.c_str(), file.size(), &w, &h))
	//		return 0;

	//	const auto& tex = ren::draw.textures[next_free_id] = std::make_shared<ren::texture>(texture.data(), w, h, w * 4);
	//	me->add_texture(next_free_id);
	//	api.texture.on_take(next_free_id);
	//	tex->create();

	//	s.push(next_free_id);
	//	return 1;
	//}

	int set_frame(lua_State *l)
	{
		runtime_state s(l);
		const auto r = s.check_arguments({
			{ltd::user_data},
			{ltd::number},
		});

		if (!r)
		{
			s.error(XOR("usage: render.set_frame(tex, frame)"));
			return 0;
		}

		const auto id = s.get_uint64(1);
		if (!ren::draw.textures[id])
		{
			s.error(XOR("the requested texture is not loaded"));
			return 0;
		}

		const auto &tex = ren::draw.textures[id]->as<ren::animated_texture>();
		if (!tex->is_animated)
			return 0;

		tex->set_frame((uint32_t)s.get_integer(2));
		return 0;
	}

	int get_frame_count(lua_State *l)
	{
		runtime_state s(l);
		const auto r = s.check_arguments({
			{ltd::user_data},
		});

		if (!r)
		{
			s.error(XOR("usage: render.set_frame(tex)"));
			return 0;
		}

		const auto id = s.get_uint64(1);
		if (!ren::draw.textures[id])
		{
			s.error(XOR("the requested texture is not loaded"));
			return 0;
		}

		const auto &tex = ren::draw.textures[id]->as<ren::animated_texture>();
		if (!tex->is_animated)
		{
			s.push(0);
			return 1;
		}

		s.push((int)tex->get_frame_count());
		return 1;
	}

	int set_loop(lua_State *l)
	{
		runtime_state s(l);
		const auto r = s.check_arguments({
			{ltd::user_data},
			{ltd::boolean},
		});

		if (!r)
		{
			s.error(XOR("usage: render.set_loop(tex, value)"));
			return 0;
		}

		const auto id = s.get_uint64(1);
		if (!ren::draw.textures[id])
		{
			s.error(XOR("the requested texture is not loaded"));
			return 0;
		}

		const auto &tex = ren::draw.textures[id]->as<ren::animated_texture>();
		if (!tex->is_animated)
			return 0;

		tex->should_loop = s.get_boolean(2);
		return 0;
	}

	int reset_loop(lua_State *l)
	{
		runtime_state s(l);
		const auto r = s.check_arguments({
			{ltd::user_data},
		});

		if (!r)
		{
			s.error(XOR("usage: render.reset_loop(tex)"));
			return 0;
		}

		const auto id = s.get_uint64(1);
		if (!ren::draw.textures[id])
		{
			s.error(XOR("the requested texture is not loaded"));
			return 0;
		}

		const auto &tex = ren::draw.textures[id]->as<ren::animated_texture>();
		if (!tex->is_animated)
			return 0;

		tex->reset_loop();
		return 0;
	}

	int set_texture(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		const auto r = s.check_arguments({
			{ltd::user_data, false, true},
		});

		if (!r)
		{
			s.error(XOR("usage: set_texture(tex)"));
			return 0;
		}

		if (s.is_nil(1))
		{
			draw_mgr.buf->g.texture = {};
			return 0;
		}

		const auto id = s.get_uint64(1);
		if (id == GUI_HASH("back_buffer"))
		{
			draw_mgr.buf->g.texture = ren::draw.adapter->get_back_buffer();
			return 0;
		}
		else if (id == GUI_HASH("back_buffer_downsampled"))
		{
			draw_mgr.buf->g.texture = ren::draw.adapter->get_back_buffer_downsampled();
			return 0;
		}

		if (!ren::draw.textures[id])
		{
			s.error(XOR("the requested texture is not loaded"));
			return 0;
		}

		draw_mgr.buf->g.texture = ren::draw.textures[id]->obj;
		return 0;
	}

	int pop_texture(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		draw_mgr.buf->g.texture = {};
		return 0;
	}

	int push_clip_rect(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		const auto r = s.check_arguments({
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::boolean, true},
		});

		if (!r)
		{
			s.error(XOR("usage: push_clip_rect(x1, y1, x2, y2, intersect = true)"));
			return 0;
		}

		const auto intersect = s.get_stack_top() == 5 ? s.get_boolean(5) : true;

		api.last_clip = draw_mgr.buf->g.clip_rect;
		draw_mgr.buf->override_clip_rect(ren::rect{s.get_float(1), s.get_float(2), s.get_float(3), s.get_float(4)},
										 intersect);
		return 0;
	}

	int pop_clip_rect(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		draw_mgr.buf->g.clip_rect = api.last_clip;
		api.last_clip = {};

		return 0;
	}

	int set_uv(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		const auto r = s.check_arguments({
			{ltd::number, false, true},
			{ltd::number, true},
			{ltd::number, true},
			{ltd::number, true},
		});

		if (!r)
		{
			s.error(XOR("usage: set_uv(x1, y1, x2, y2)"));
			return 0;
		}

		if (s.is_nil(1))
		{
			draw_mgr.buf->g.uv_rect = {};
			return 0;
		}

		draw_mgr.buf->g.uv_rect = ren::rect{s.get_float(1), s.get_float(2), s.get_float(3), s.get_float(4)};
		return 0;
	}

	int pop_uv(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		draw_mgr.buf->g.uv_rect = {};
		return 0;
	}

	int create_animator_color(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({
			{ltd::table},
			{ltd::number},
			{ltd::number, true},
			{ltd::boolean, true},
		});

		if (!r)
		{
			s.error(XOR(
				"usage: create_animator_color(initial, time, interpolation = linear, interp_hue = false): anim_color"));
			return 0;
		}

		const auto me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		if (s.get_float(2) <= 0.f)
		{
			s.error(XOR("duration must be greater than 0"));
			return 0;
		}

		const auto interp = s.get_stack_top() == 3 ? s.get_integer(3) : ren::ease_linear;
		if (interp < 0 || interp >= ren::ease_max)
		{
			s.error(XOR("invalid interpolation type"));
			return 0;
		}

		const auto is_hue = s.get_stack_top() >= 4 ? s.get_boolean(4) : false;

		const auto anim = std::make_shared<ren::anim_color>(extract_color(s, 1), s.get_float(2), interp);
		anim->type = is_hue ? ren::act_hsva : ren::act_rgba;

		const auto next_free_id = api.animator.get_empty_slot();
		api.anims[next_free_id] = anim;
		me->add_animator(next_free_id);
		api.animator.on_take(next_free_id);

		std::weak_ptr<ren::anim_color> obj{anim};
		s.create_user_object<decltype(obj)>(XOR("render.anim_color"), &obj);

		return 1;
	}

	int anim_color_direct(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({
			{ltd::user_data},
			{ltd::table},
			{ltd::table, true},
		});

		if (!r)
		{
			s.error(XOR("usage: obj:direct(to) or obj:direct(from, to)"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<ren::anim_color> *>(s.get_user_data(1));
		const auto anim = obj.lock();
		if (!anim)
		{
			s.error(XOR("invalid animator"));
			return 0;
		}

		if (s.get_stack_top() == 3)
			anim->direct(extract_color(s, 2), extract_color(s, 3));
		else
			anim->direct(extract_color(s, 2));

		return 0;
	}

	int anim_color_get_value(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({
			{ltd::user_data},
		});

		if (!r)
		{
			s.error(XOR("usage: obj:get_value()"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<ren::anim_color> *>(s.get_user_data(1));
		const auto anim = obj.lock();
		if (!anim)
		{
			s.error(XOR("invalid animator"));
			return 0;
		}

		s.create_table();
		s.set_field(XOR("r"), anim->value.value.r * 255.f);
		s.set_field(XOR("g"), anim->value.value.g * 255.f);
		s.set_field(XOR("b"), anim->value.value.b * 255.f);
		s.set_field(XOR("a"), anim->value.value.a * 255.f);

		return 1;
	}

	int create_animator_float(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({
			{ltd::number},
			{ltd::number},
			{ltd::number, true},
		});

		if (!r)
		{
			s.error(XOR("usage: create_animator_float(initial, time, interpolation = linear): anim_float"));
			return 0;
		}

		const auto me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		if (s.get_float(2) <= 0.f)
		{
			s.error(XOR("duration must be greater than 0"));
			return 0;
		}

		const auto interp = s.get_stack_top() == 3 ? s.get_integer(3) : ren::ease_linear;
		if (interp < 0 || interp >= ren::ease_max)
		{
			s.error(XOR("invalid interpolation type"));
			return 0;
		}

		const auto anim = std::make_shared<ren::anim_float>(s.get_float(1), s.get_float(2), interp);

		const auto next_free_id = api.animator.get_empty_slot();
		api.anims[next_free_id] = anim;
		me->add_animator(next_free_id);
		api.animator.on_take(next_free_id);

		std::weak_ptr<ren::anim_float> obj{anim};
		s.create_user_object<decltype(obj)>(XOR("render.anim_float"), &obj);

		return 1;
	}

	int anim_float_direct(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({
			{ltd::user_data},
			{ltd::number},
			{ltd::number, true},
		});

		if (!r)
		{
			s.error(XOR("usage: obj:direct(to) or obj:direct(from, to)"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<ren::anim_float> *>(s.get_user_data(1));
		const auto anim = obj.lock();
		if (!anim)
		{
			s.error(XOR("invalid animator"));
			return 0;
		}

		if (s.get_stack_top() == 3)
			anim->direct(s.get_float(2), s.get_float(3));
		else
			anim->direct(s.get_float(2));

		return 0;
	}

	int anim_float_get_value(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({
			{ltd::user_data},
		});

		if (!r)
		{
			s.error(XOR("usage: obj:get_value()"));
			return 0;
		}

		const auto obj = *reinterpret_cast<std::weak_ptr<ren::anim_float> *>(s.get_user_data(1));
		const auto anim = obj.lock();
		if (!anim)
		{
			s.error(XOR("invalid animator"));
			return 0;
		}

		s.push(anim->value);
		return 1;
	}

	int get_texture_size(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({
			{ltd::user_data},
		});

		if (!r)
		{
			s.error(XOR("usage: get_texture_size(tex): number, number"));
			return 0;
		}

		const auto id = s.get_uint64(1);
		if (!ren::draw.textures[id])
		{
			s.error(XOR("the requested texture is not loaded"));
			return 0;
		}

		const auto tex = ren::draw.textures[id];
		const auto size = tex->get_size();

		s.push(size.x);
		s.push(size.y);

		return 2;
	}

	int get_text_size(lua_State *l)
	{
		runtime_state s(l);

		const auto r = s.check_arguments({{ltd::user_data}, {ltd::string}});

		if (!r)
		{
			s.error(XOR("usage: get_text_size(font, text): number, number"));
			return 0;
		}

		if (!ren::draw.fonts[s.get_uint64(1)])
		{
			s.error(XOR("the requested font is not loaded"));
			return 0;
		}

		const auto font = ren::draw.fonts[s.get_uint64(1)];
		const auto size = font->get_text_size(s.get_string(2));

		s.push(size.x);
		s.push(size.y);

		return 2;
	}

	int color(lua_State *l)
	{
		runtime_state s(l);

		auto r = s.check_arguments({{ltd::number}, {ltd::number}, {ltd::number}, {ltd::number, true}});

		int cr, cg, cb, ca;
		if (!r)
		{
			r = s.check_arguments({{ltd::string}});

			if (!r)
			{
				s.error(XOR("usage: color(r, g, b, a = 255) or color(hex)"));
				return 0;
			}

			auto parse_c = sscanf_s(s.get_string(1), XOR("#%02x%02x%02x%02x"), &cr, &cg, &cb, &ca);
			if (parse_c < 3)
			{
				int crl, cgl, cbl, cal;
				parse_c = sscanf_s(s.get_string(1), XOR("#%01x%01x%01x%01x"), &crl, &cgl, &cbl, &cal);
				if (parse_c < 3)
				{
					s.error(XOR("invalid hex color (example: #00acf5, format: #RRGGBBAA)"));
					return 0;
				}

				cr = crl | (crl << 4);
				cg = cgl | (cgl << 4);
				cb = cbl | (cbl << 4);

				if (parse_c == 4)
					ca = cal | (cal << 4);
			}

			if (parse_c != 4)
				ca = 255;
		}
		else
		{
			cr = s.get_integer(1);
			cg = s.get_integer(2);
			cb = s.get_integer(3);
			ca = s.get_stack_top() == 4 ? s.get_integer(4) : 255;
		}

		s.create_table();
		s.set_field(XOR("r"), cr);
		s.set_field(XOR("g"), cg);
		s.set_field(XOR("b"), cb);
		s.set_field(XOR("a"), ca);

		return 1;
	}

	int rect_filled(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		const auto r = s.check_arguments({{ltd::number}, {ltd::number}, {ltd::number}, {ltd::number}, {ltd::table}});

		if (!r)
		{
			s.error(XOR("usage: rect_filled(x1, y1, x2, y2, color: { r, g, b, a })"));
			return 0;
		}

		draw_mgr.buf->add_rect_filled(ren::rect(s.get_float(1), s.get_float(2), s.get_float(3), s.get_float(4)),
							 extract_color(s));
		return 0;
	}

	int rect_filled_multicolor(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		const auto r = s.check_arguments({
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::table},
			{ltd::table},
			{ltd::table},
			{ltd::table},
		});

		if (!r)
		{
			s.error(XOR("usage: rect_filled_multicolor(x1, y1, x2, y2, tl, tr, br, bl)"));
			return 0;
		}

		draw_mgr.buf->add_rect_filled_multicolor(
			ren::rect{s.get_float(1), s.get_float(2), s.get_float(3), s.get_float(4)},
										{
											extract_color(s, 5),
											extract_color(s, 6),
											extract_color(s, 7),
											extract_color(s, 8),
										});

		return 0;
	}

	int line(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		const auto r = s.check_arguments({
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::table},
		});

		if (!r)
		{
			s.error(XOR("usage: line(x1, y1, x2, y2, color)"));
			return 0;
		}

		draw_mgr.buf->add_line({s.get_float(1), s.get_float(2)}, {s.get_float(3), s.get_float(4)}, extract_color(s));
		return 0;
	}

	int line_multicolor(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		const auto r = s.check_arguments({
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::table},
		});

		if (!r)
		{
			s.error(XOR("usage: line_multicolor(x1, y1, x2, y2, color, color2)"));
			return 0;
		}

		draw_mgr.buf->add_line_multicolor({s.get_float(1), s.get_float(2)}, {s.get_float(3), s.get_float(4)},
										  extract_color(s),
								 extract_color(s, 6));
		return 0;
	}

	int triangle(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		const auto r = s.check_arguments({
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::table},
			{ltd::number, true},
			{ltd::number, true},
		});

		if (!r)
		{
			s.error(XOR("usage: triangle(x1, y1, x2, y2, x3, y3, color, thickness, outline)"));
			return 0;
		}

		draw_mgr.buf->add_triangle({s.get_float(1), s.get_float(2)}, {s.get_float(3), s.get_float(4)},
						  {s.get_float(5), s.get_float(6)}, extract_color(s, 7),
						  s.get_stack_top() >= 8 ? std::clamp(s.get_float(8), 1.f, FLT_MAX) : 1.f,
						  s.get_stack_top() >= 9 ? (ren::outline_mode)s.get_integer(9) : ren::outline_inset);
		return 0;
	}

	int triangle_filled(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		const auto r = s.check_arguments({
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::table},
		});

		if (!r)
		{
			s.error(XOR("usage: triangle_filled(x1, y1, x2, y2, x3, y3, color)"));
			return 0;
		}

		draw_mgr.buf->add_triangle_filled({s.get_float(1), s.get_float(2)}, {s.get_float(3), s.get_float(4)},
								 {s.get_float(5), s.get_float(6)}, extract_color(s, 7));
		return 0;
	}

	int triangle_filled_multicolor(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		const auto r = s.check_arguments({
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::table},
			{ltd::table},
			{ltd::table},
		});

		if (!r)
		{
			s.error(XOR("usage: triangle_filled(x1, y1, x2, y2, x3, y3, col1, col2, col3)"));
			return 0;
		}

		draw_mgr.buf->add_triangle_filled_multicolor({s.get_float(1), s.get_float(2)}, {s.get_float(3), s.get_float(4)},
											{s.get_float(5), s.get_float(6)},
											{
												extract_color(s, 7),
												extract_color(s, 8),
												extract_color(s, 9),
											});

		return 0;
	}

	int rect(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		const auto r = s.check_arguments({{ltd::number},
										  {ltd::number},
										  {ltd::number},
										  {ltd::number},
										  {ltd::table},
										  {ltd::number, true},
										  {ltd::number, true}});

		if (!r)
		{
			s.error(XOR("usage: rect(x1, y1, x2, y2, color: { r, g, b, a }, thickness, outline)"));
			return 0;
		}

		draw_mgr.buf->add_rect(ren::rect(s.get_float(1), s.get_float(2), s.get_float(3), s.get_float(4)),
							   extract_color(s),
					  s.get_stack_top() >= 6 ? std::clamp(s.get_float(6), 1.f, FLT_MAX) : 1.f,
					  s.get_stack_top() >= 7 ? (ren::outline_mode)s.get_integer(7) : ren::outline_inset);

		return 0;
	}

	int rect_rounded(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		if (!s.check_arguments({{ltd::number},
								{ltd::number},
								{ltd::number},
								{ltd::number},
								{ltd::table},
								{ltd::number},
								{ltd::number, true},
								{ltd::number, true},
								{ltd::number, true}}))
		{
			s.error(XOR("usage: rect_rounded(x1, y1, x2, y2, color, rounding, sides, thickness, outline)"));
			return 0;
		}

		draw_mgr.buf->add_rect_rounded(
			ren::rect(s.get_float(1), s.get_float(2), s.get_float(3), s.get_float(4)),
							  extract_color(s),
							  s.get_float(6), s.get_stack_top() >= 7 ? (ren::rounding)s.get_integer(7) : ren::rnd_all,
							  s.get_stack_top() >= 8 ? std::clamp(s.get_float(8), 1.f, FLT_MAX) : 1.f,
							  s.get_stack_top() >= 9 ? (ren::outline_mode)s.get_integer(9) : ren::outline_inset);

		return 0;
	}

	int rect_filled_rounded(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		const auto r = s.check_arguments(
		{{ltd::number}, {ltd::number}, {ltd::number}, {ltd::number}, {ltd::table}, {ltd::number},
		 {ltd::number, true}});

		if (!r)
		{
			s.error(XOR("usage: rect_filled_rounded(x1, y1, x2, y2, color: { r, g, b, a }, rounding, sides = all)"));
			return 0;
		}

		const auto rounding = static_cast<char>(s.get_stack_top() == 7 ? s.get_integer(7) : ren::rnd_all);

		draw_mgr.buf->g.anti_alias = true;
		draw_mgr.buf->add_rect_filled_rounded(ren::rect(s.get_float(1), s.get_float(2), s.get_float(3), s.get_float(4)),
									 extract_color(s), s.get_float(6), (ren::rounding)rounding);
		draw_mgr.buf->g.anti_alias = false;

		return 0;
	}

	int text(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		const auto r = s.check_arguments({
			{ltd::user_data},
			{ltd::number},
			{ltd::number},
			{ltd::string},
			{ltd::table},
			{ltd::number, true},
			{ltd::number, true},
			{ltd::number, true},
		});

		if (!r)
		{
			s.error(XOR(
				"usage: text(font, x, y, text, color: { r, g, b, a }, align_h = left, align_v = top, align_l = left)"));
			return 0;
		}

		if (!ren::draw.fonts[s.get_uint64(1)])
		{
			s.error(XOR("the requested font is not loaded"));
			return 0;
		}

		draw_mgr.buf->font = ren::draw.fonts[s.get_uint64(1)];

		ren::text_alignment ah{ren::align_left}, av{ren::align_top}, al{ren::align_left};
		if (s.get_stack_top() > 5)
		{
			if (s.get_stack_top() >= 6)
				ah = (ren::text_alignment)s.get_integer(6);
			if (s.get_stack_top() >= 7)
				av = (ren::text_alignment)s.get_integer(7);
			if (s.get_stack_top() >= 8)
				al = (ren::text_alignment)s.get_integer(8);
		}

		draw_mgr.buf->add_text({s.get_float(2), s.get_float(3)}, s.get_string(4), extract_color(s),
					  ren::text_params::with_vhline(av, ah, al));

		return 0;
	}

	int set_rotation(lua_State *l)
	{
		runtime_state s(l);
		if (!s.check_arguments({{ltd::number}}))
		{
			s.error(XOR("usage: render.set_rotation(rot)"));
			return 0;
		}

		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		draw_mgr.buf->g.rotation = s.get_float(1);
		return 0;
	}

	int wrap_text(lua_State *l)
	{
		runtime_state s(l);
		const auto r = s.check_arguments({{ltd::user_data}, {ltd::string}, {ltd::number}});

		if (!r)
		{
			s.error(XOR("usage: render.wrap_text(font, text, width)"));
			return 0;
		}

		if (!ren::draw.fonts[s.get_uint64(1)])
		{
			s.error(XOR("the requested font is not loaded"));
			return 0;
		}

		const auto &f = ren::draw.fonts[s.get_uint64(1)];
		s.push(f->wrap_text(s.get_string(2), s.get_float(3)));
		return 1;
	}

	int get_screen_size(lua_State *l)
	{
		runtime_state s(l);

		const auto sz = ren::draw.display;
		s.push(sz.x);
		s.push(sz.y);

		return 2;
	}

	int circle(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		const auto r = s.check_arguments({
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::table},
			{ltd::number, true},
			{ltd::number, true},
			{ltd::number, true},
			{ltd::number, true},
		});

		if (!r)
		{
			s.error(XOR(
				"usage: circle(x, y, radius, color: { r, g, b, a }, segments = 36, fill = 1.0, rot = 0.0, thickness = 1)"));
			return 0;
		}

		const auto thickness = s.get_stack_top() >= 8 ? std::clamp(s.get_float(8), 1.f, FLT_MAX) : 1.f;
		const auto outline = s.get_stack_top() >= 9 ? (ren::outline_mode)s.get_integer(9) : ren::outline_inset;
		const auto segments = s.get_stack_top() >= 5 ? s.get_integer(5) : 36;
		const auto fill = s.get_stack_top() >= 6 ? s.get_float(6) : 1.f;
		const auto rot = s.get_stack_top() >= 7 ? s.get_float(7) : 0.f;

		draw_mgr.buf->g.anti_alias = true;
		draw_mgr.buf->g.rotation = rot;
		draw_mgr.buf->add_circle({s.get_float(1), s.get_float(2)}, s.get_float(3), extract_color(s, 4), segments, fill,
						thickness,
						outline);
		draw_mgr.buf->g.rotation = 0.f;
		draw_mgr.buf->g.anti_alias = false;

		return 0;
	}

	int circle_filled(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		const auto r = s.check_arguments({
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::table},
			{ltd::number, true},
			{ltd::number, true},
			{ltd::number, true},
		});

		if (!r)
		{
			s.error(
				XOR("usage: circle_filled(x, y, radius, color: { r, g, b, a }, segments = 36, fill = 1.0, rot = 0.0)"));
			return 0;
		}

		const auto segments = s.get_stack_top() >= 5 ? s.get_integer(5) : 36;
		const auto fill = s.get_stack_top() >= 6 ? s.get_float(6) : 1.f;
		const auto rot = s.get_stack_top() >= 7 ? s.get_float(7) : 0.f;

		draw_mgr.buf->g.anti_alias = true;
		draw_mgr.buf->g.rotation = rot;
		draw_mgr.buf->add_circle_filled({s.get_float(1), s.get_float(2)}, s.get_float(3), extract_color(s, 4), segments,
										fill);
		draw_mgr.buf->g.rotation = 0.f;
		draw_mgr.buf->g.anti_alias = false;

		return 0;
	}

	int circle_filled_multicolor(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		const auto r = s.check_arguments({
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::table},
			{ltd::table},
			{ltd::number, true},
			{ltd::number, true},
			{ltd::number, true},
		});

		if (!r)
		{
			s.error(XOR("usage: circle_filled(x, y, radius, a, b, segments = 36, fill = 1.0, rot = 0.0)"));
			return 0;
		}

		const auto segments = s.get_stack_top() >= 6 ? s.get_integer(6) : 36;
		const auto fill = s.get_stack_top() >= 7 ? s.get_float(7) : 1.f;
		const auto rot = s.get_stack_top() >= 8 ? s.get_float(8) : 0.f;

		draw_mgr.buf->g.anti_alias = true;
		draw_mgr.buf->g.rotation = rot;
		draw_mgr.buf->add_circle_filled_multicolor({s.get_float(1), s.get_float(2)}, s.get_float(3),
										  {extract_color(s, 4), extract_color(s, 5)}, segments, fill);
		draw_mgr.buf->g.rotation = 0.f;
		draw_mgr.buf->g.anti_alias = false;

		return 0;
	}

	int esp_text(lua_State *l)
	{
		runtime_state s(l);
		s.error(XOR("todo: esp_text"));
		return 0;
	}

	int esp_bar(lua_State *l)
	{
		runtime_state s(l);
		s.error(XOR("todo: esp_bar"));
		return 0;
	}

	int esp_icon(lua_State *l)
	{
		runtime_state s(l);
		s.error(XOR("todo: esp_icon"));
		return 0;
	}

	int set_alpha(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		if (!s.check_arguments({{ltd::number}}))
		{
			s.error(XOR("usage: render.set_alpha(alpha)"));
			return 0;
		}

		draw_mgr.buf->g.alpha = s.get_float(1);
		return 0;
	}

	int get_alpha(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		s.push(draw_mgr.buf->g.alpha);
		return 1;
	}

	int pop_alpha(lua_State *l)
	{
		runtime_state s(l);
		if (!api.in_render)
		{
			s.error(XOR("this function can only be called from on_paint()!"));
			return 0;
		}

		draw_mgr.buf->g.alpha = 1.f;
		return 0;
	}

	int blur(lua_State *l)
	{
		runtime_state s(l);
		const auto r = s.check_arguments({
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::number},
			{ltd::function},
		});

		if (!r)
		{
			s.error(XOR("usage: render.blur(x0, y0, x1, y1, fn)"));
			return 0;
		}

		const auto me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		const auto fn = s.registry_add();
		evo::ren::add_with_blur(draw_mgr.buf, {s.get_float(1), s.get_float(2), s.get_float(3), s.get_float(4)},
								[&l, &me, &fn](auto &d)
								{
									runtime_state s(l);
									s.registry_get(fn);
									if (!s.is_function(-1))
									{
										s.registry_remove(fn);
										return;
									}

									if (!s.call(0, 0))
									{
										me->did_error = true;
										lua::helpers::error(XOR("runtime_error"),
															s.get_last_error_description().c_str());
										if (const auto f = api.find_script_file(me->id); f.has_value())
											f->get().should_unload = true;

										return;
									}
								});
		s.registry_remove(fn);

		return 0;
	}

	int create_shader(lua_State *l)
	{
		runtime_state s(l);
		if (!s.check_arguments({{ltd::string}}))
		{
			s.error(XOR("usage: render.create_shader(src)"));
			return 0;
		}

		const auto me = api.find_by_state(l);
		if (!me)
		{
			s.error(XOR("FATAL: could not find the script. Did it escape the sandbox?"));
			return 0;
		}

		const auto next_free_id = api.shader.get_empty_slot();
		const auto &shd = ren::draw.shaders[next_free_id] =
			std::make_shared<ren::shader>(s.get_string(1), ren::shader_frag);
		me->add_shader(next_free_id);
		api.shader.on_take(next_free_id);
		shd->create();

		s.push(next_free_id);
		return 1;
	}

	int set_shader(lua_State *l)
	{
		runtime_state s(l);
		if (!s.check_arguments({{ltd::user_data, false, true}}))
		{
			s.error(XOR("usage: render.set_shader(shader)"));
			return 0;
		}

		if (s.is_nil(1))
		{
			draw_mgr.buf->g.frag_shader = {};
			return 0;
		}

		const auto id = s.get_uint64(1);
		if (!ren::draw.shaders[id])
		{
			s.error(XOR("the requested shader is not loaded"));
			return 0;
		}

		draw_mgr.buf->g.frag_shader = ren::draw.shaders[id]->obj;
		return 0;
	}

	int set_anti_aliasing(lua_State *l)
	{
		runtime_state s(l);
		if (!s.check_arguments({{ltd::boolean}}))
		{
			s.error(XOR("usage: render.set_anti_aliasing(val)"));
			return 0;
		}

		draw_mgr.buf->g.anti_alias = s.get_boolean(1);
		return 0;
	}

	int set_render_mode(lua_State *l)
	{
		runtime_state s(l);
		if (!s.check_arguments({{ltd::number}}))
		{
			s.error(XOR("usage: render.set_render_mode(mode)"));
			return 0;
		}

		draw_mgr.buf->g.mode = (ren::render_mode)s.get_integer(1);
		return 0;
	}
} // namespace lua::api_def::render
