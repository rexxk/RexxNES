#include "input/controller.h"

#include <array>


namespace emu
{


	static std::array<bool, 8> ButtonState;


	auto Controller::SetState(Button button, bool pressed) -> void
	{
		ButtonState[static_cast<uint8_t>(button)] = pressed;
	}

	auto Controller::GetButtonBits() -> std::uint8_t
	{
		std::uint8_t bitfield{};

		for (std::uint8_t i = 0; i < 8; i++)
		{
			if (ButtonState[i])
				bitfield |= 1 << i;
		}

		return bitfield;
	}

}
