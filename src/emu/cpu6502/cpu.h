#pragma once

#include <cstdint>
#include <span>


namespace emu
{


	class CPU
	{
	public:
		CPU() = default;

		auto InstallROM(std::uint16_t address, std::span<uint8_t> romData) -> void;
		auto SetMemory(std::uint16_t location, std::uint8_t value) -> void;
		auto Execute() -> void;
//		auto Execute(std::span<std::uint8_t> program, const std::uint16_t memoryLocation) -> void;

	private:

		std::uint16_t m_StartVector{ 0 };

	};


}
