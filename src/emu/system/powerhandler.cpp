#include "powerhandler.h"



namespace emu
{


	PowerHandler::PowerHandler(PowerState initialState)
	{
		m_State.store(initialState);
	}

	auto PowerHandler::SetState(PowerState state) -> void
	{
		m_State.store(state);
	}



}
