#include <std_include.hpp>

#include "components/asset_limits.hpp"
#include "game/game.hpp"

namespace lua
{
	struct lua_State;
}

extern "C"
{
	int __declspec(dllexport) init(lua::lua_State*)
	{
		game::minlog.Open("T7Overclocked.log", "logs");

		const auto build = game::offsets::active_build();
		if (build == game::offsets::build::unknown)
		{
			const char* message =
				"Unsupported Black Ops 3 executable. No T7Overclocked asset-pool hooks were installed.";
			game::minlog.WriteLine(message);
			MessageBoxA(nullptr, message, "T7Overclocked", MB_OK | MB_ICONERROR);
			return 0;
		}

		game::minlog.WriteLine(game::offsets::build_name());
		if (!asset_limits::initialize())
			game::minlog.WriteLine("Asset-pool initialization did not complete; see the preceding log messages.");
		return 1;
	}
}
