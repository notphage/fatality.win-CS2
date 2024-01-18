#include <macros.h>
#include <game/draw_manager.h>
#include <game/game.h>
#include <game/hook_manager.h>
#include <gui/gui.h>
#include <menu/menu.h>
#include <menu/init/init_anim.h>
#include <ren/renderer.h>
#include <ren/adapters/adapter_dx11.h>
#include <sdk/globalvars.h>

namespace hooks::steam
{
	using namespace evo::ren;
	using namespace evo::gui;

	HRESULT present(IDXGISwapChain *chain, UINT sync, UINT flags)
	{
		static ID3D11RenderTargetView *view;

		if (!draw.adapter)
		{
			// fix dx11 memes.
			ID3D11Device *dev{};
			ID3D11DeviceContext *ctx{};

			chain->GetDevice(__uuidof(ID3D11Device), (void **)&dev);
			dev->GetImmediateContext(&ctx);

			ID3D11Texture2D *back_buffer;
			chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&back_buffer));

			D3D11_RENDER_TARGET_VIEW_DESC desc{};
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
			dev->CreateRenderTargetView(back_buffer, &desc, &view);

			back_buffer->Release();

			DXGI_SWAP_CHAIN_DESC chain_desc;
			chain->GetDesc(&chain_desc);

			draw.adapter = std::make_shared<adapter_dx11>(chain_desc.OutputWindow, dev, ctx);
			draw.create_layer(game_layer);
			draw.create_layer(17);
			draw.create_layer(63);

			// copy esp layer and make it deferred
			draw_mgr.buf = draw.layers[game_layer];
			draw_mgr.buf->is_deferred = true;
			draw_mgr.buf->skip_dpi = true;

			menu::init_anim.on_adapter_ready();

			draw.refresh();
			draw.create_objects();
		}

		if (menu::init_anim.init_ready && !menu::init_anim.init_done)
		{
			draw.scale = std::clamp(draw.display.y, 1.f, FLT_MAX) / 1080.f;
			ctx = std::make_unique<context>();

			char win_dir_arr[256]{};
			GetSystemWindowsDirectoryA(win_dir_arr, 256);

			const std::string win_dir{win_dir_arr};
			draw.fonts[FNV1A("esp")] = std::make_shared<font_gdi>(xorstr_("Smallest Pixel-7"), 11.f, font_flag_outline | font_flag_no_dpi, 0, 0x45F);
			draw.fonts[FNV1A("esp_name")] = std::make_shared<font>(
				(win_dir + xorstr_("/fonts/segoeuib.ttf")).c_str(), 13.f, font_flag_shadow | font_flag_no_dpi, 0, 0x45F);
			draw.fonts[FNV1A("lby")] = std::make_shared<font>((win_dir + xorstr_("/fonts/verdanab.ttf")).c_str(), 27.f, font_flag_shadow, 0, 0x80);
			menu::men.init();

			draw.refresh();
			draw.create_objects();

			menu::init_anim.init_done = true;
		}

		static_cast<ID3D11DeviceContext *>(draw.adapter->get_context())->OMSetRenderTargets(1, &view, nullptr);

		draw.time = game->global_vars->real_time;
		draw.frame_time = game->global_vars->frame_time;

		draw.refresh();

		if (menu::init_anim.init_done)
		{
			if (menu::men.full_cycle_reset)
			{
				menu::men.full_cycle_reset = false;
				menu::group::rebuild_legit_layout();
				menu::group::rebuild_rage_layout();
				ctx->reset();
				ctx->should_update_old_value = false;
			}

			std::lock_guard lock(menu::men.render_mtx);
			draw.begin_frame();
			ctx->render();

			menu::men.finalize();
		}
		else
		{
			draw.begin_frame();
			menu::init_anim.render();
		}

		draw.end_frame();

		//handle synced texture resets
		if (draw.layers[game_layer]->deferred_dirty && draw_mgr.dirty_ackn)
		{
			draw.layers[game_layer]->cb->reset(true);
			draw.layers[game_layer]->deferred_dirty = false;
			draw_mgr.dirty_ackn = false;
		}

		draw.render();
		draw.flush();

		// handle safe font reset
		if (menu::men.font_reset)
		{
			draw.layers[game_layer]->deferred_dirty = true;
			// update fonts
			for (auto &[_, f] : draw.fonts)
			{
				if (!f)
					continue;

				f->destroy();
				f->create();
			}

			menu::men.font_was_reset = true;
			menu::men.font_reset = false;
		}

		return hook_manager.present->call(chain, sync, flags);
	}
} // namespace hooks::steam
