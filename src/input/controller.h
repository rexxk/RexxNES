#pragma once

#include <cstdint>


namespace emu
{

	enum class Button
	{
		Right = 0,
		Left,
		Down,
		Up,
		Start,
		Select,
		B,
		A,
	};

	class Controller
	{
	public:
		static auto SetState(Button button, bool pressed) -> void;

		static auto GetButtonBits() -> std::uint8_t;

	};

}
