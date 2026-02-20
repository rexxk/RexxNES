#pragma once

#include <cstdint>


namespace emu
{

	enum class Button
	{
		A = 0,
		B,
		Select,
		Start,
		Up,
		Down,
		Left,
		Right,
	};

	class Controller
	{
	public:
		static auto SetState(Button button, bool pressed) -> void;

		static auto GetButtonBits() -> std::uint8_t;

	};

}
