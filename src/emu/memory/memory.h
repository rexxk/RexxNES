#pragma once

#include <cstdint>
#include <span>
#include <vector>


namespace emu
{


	class Memory
	{
	public:
		explicit Memory(size_t sizeKilobytes);

		auto InstallROM(std::uint16_t address, std::span<std::uint8_t> romData) -> void;

		auto Read(std::uint16_t address) -> std::uint8_t;
		auto Write(std::uint16_t address, std::uint8_t value) -> void;

		auto ViewPage(uint8_t page) -> void;

	private:
		const size_t m_Size{ };

		std::vector<std::uint8_t> m_Data{};
	};


}
