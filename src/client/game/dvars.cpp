#include <std_include.hpp>
#include "game.hpp"
#include "dvars.hpp"

namespace dvars
{
	game::dvar_t* ui_error_report = nullptr;
	game::dvar_t* ui_error_report_delay = nullptr;
	game::dvar_t* ui_error_callstack_ship = nullptr;
	game::dvar_t* sv_mapname = nullptr;
	game::dvar_t* ui_mapname = nullptr;
	game::dvar_t* ui_gametype = nullptr;
	game::dvar_t* com_maxclients = nullptr;

	void initialize()
	{
		ui_error_report = *reinterpret_cast<game::dvar_t**>(OFFSET(0x15686ff30));
		ui_error_report_delay = *reinterpret_cast<game::dvar_t**>(OFFSET(0x15686ff38));
		ui_error_callstack_ship = *reinterpret_cast<game::dvar_t**>(OFFSET(0x15686ed20));
		sv_mapname = *reinterpret_cast<game::dvar_t**>(OFFSET(0x157736848));
		ui_mapname = *reinterpret_cast<game::dvar_t**>(OFFSET(0x157960bc0));
		ui_gametype = *reinterpret_cast<game::dvar_t**>(OFFSET(0x157960bd0));
		com_maxclients = *reinterpret_cast<game::dvar_t**>(OFFSET(0x15686e8d0));
	}
}
