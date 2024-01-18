#include "game/game.h"

BOOL APIENTRY DllMain(uintptr_t inst, uint32_t reason, uint32_t reserved)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		std::thread(
			[inst, reserved]
			{
				game = std::make_unique<game_t>(inst, reserved);
				game->init();
			}).detach();
	}

	return TRUE;
}
