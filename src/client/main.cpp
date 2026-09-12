#include <std_include.hpp>

#include "game/game.hpp"
#include "game/dvars.hpp"
#include "havok/hks_api.hpp"
#include "loader/component_loader.hpp"

extern "C"
{
	int __declspec(dllexport) init(lua::lua_State* L)
	{
		if (game::offsets::active_build() == game::offsets::build::unknown)
		{
			const char* message = "Unsupported Black Ops 3 executable. T7Overcharged supports March 2023, February 2026, and September 2026. No game hooks were installed.";
			game::minlog.WriteLine(message);
			MessageBoxA(nullptr, message, "T7Overcharged", MB_OK | MB_ICONERROR);
			return 0;
		}
		game::minlog.WriteLine(game::offsets::build_name());
		dvars::initialize();
		game::minlog.WriteLine("T7Overchared initiating");

		const lua::luaL_Reg T7OverchargedLibrary[] =
		{
			{nullptr, nullptr},
		};
		hks::hksI_openlib(L, "T7Overcharged", T7OverchargedLibrary, 0, 1);

		if (!component_loader::post_start())
		{
			game::Com_Error_("", 0, 0x200u, "Error while loading T7Overcharged components");
			game::minlog.WriteLine("Error while loading T7Overcharged components");
			return 0;
		}

		game::minlog.WriteLine("T7Overchared initiated");

		game::LoadDvarHashMap();
		return 1;
	}
}
