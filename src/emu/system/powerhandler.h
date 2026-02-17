#pragma once

#include <atomic>


namespace emu
{

	enum class PowerState
	{
		Off,
		Run,
		Suspended,
		SingleStep,
	};

	class PowerHandler
	{
	public:
		PowerHandler() = delete;
		explicit PowerHandler(PowerState initialState);

		auto SetState(PowerState state) -> void;
		auto GetState() -> PowerState { return m_State.load(); }

	private:
		std::atomic<PowerState> m_State;
	};


}
