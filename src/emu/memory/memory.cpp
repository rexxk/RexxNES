#include "emu/memory/memory.h"

#include <print>


namespace emu
{

	Memory::Memory(size_t sizeKilobytes)
		: m_Size(sizeKilobytes)
	{
		m_Data.resize(m_Size * 1024);
		std::println("Memory size: {} kb", m_Size);
	}

	auto Memory::InstallROM(std::uint16_t address, std::span<std::uint8_t> romData) -> void
	{
		if (address + romData.size() > m_Size * 1024)
		{
			std::println("Memory::InstallROM - Not enough space for ROM in memory at location {}", address);
			return;
		}

		std::copy(std::begin(romData), std::end(romData), m_Data.data() + address);
	}

	auto Memory::Read(std::uint16_t address) -> std::uint8_t
	{
		std::uint8_t returnValue = m_Data.at(address);

		if (address >= 0x2000 && address < 0x2FFF)
			std::println("Reading from PPU ({:04x} : {:02x})", address, m_Data.at(address));

		if (address == 0x2002 && m_Data.at(address) & 0x80)
			m_Data.at(address) &= 0x7F;
		else if (address == 0x2002)
			m_Data.at(address) |= 0x80;

		return returnValue;
	}

	auto Memory::Write(std::uint16_t address, std::uint8_t value) -> void
	{
		if (address >= 0x2000 && address < 0x2FFF)
			std::println("Writing to PPU ({:04x} = {:02x})", address, value);

		m_Data.at(address) = value;
	}

}
