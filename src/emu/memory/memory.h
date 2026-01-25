#pragma once

#include <cstdint>
#include <span>
#include <vector>


namespace emu
{


	class Memory
	{
	public:
		Memory();

		auto InstallROM(std::uint16_t address, std::span<std::uint8_t> romData) -> void;

		auto Read(std::uint16_t address) -> std::uint8_t;
		auto Write(std::uint16_t address, std::uint8_t value) -> void;

	private:
		const size_t m_Size{ 64 * 1024 };

		std::vector<std::uint8_t> m_Data{};
	};


}
