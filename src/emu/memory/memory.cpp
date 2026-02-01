#include "emu/memory/memory.h"
#include "emu/ppu/ppu.h"

#include <print>

#include <imgui.h>


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

//		if (address >= 0x2000 && address < 0x2FFF)
//			std::println("Reading from PPU ({:04x} : {:02x})", address, m_Data.at(address));
//		if (address == 0x2003 || address == 0x2004)
//			std::println("Reading from PPU ({:04x} = {:02x})", address, returnValue);

//		if (address == 0x2002 && m_Data.at(address) & 0x80)
//			m_Data.at(address) &= 0x7F;
//		else if (address == 0x2002)
//			m_Data.at(address) |= 0x80;

		if (address == 0x2002)
		{
			m_Data.at(address) &= 0x7F;
			PPU::ResetW();
		}

		return returnValue;
	}

	auto Memory::Write(std::uint16_t address, std::uint8_t value) -> void
	{
//		if (address == 0x2001)
//			std::println("Writing to PPU ({:04x} = {:02x})", address, value);

		if (address == 0x2005 || address == 0x2006)
			PPU::ToggleW();

		m_Data.at(address) = value;
	}


	auto Memory::ViewPage(uint8_t page) -> void
	{
		std::uint16_t startAddress = page * 0x100;

		for (auto row = 0; row < 16; row++)
		{
			std::string str = std::format("{:04x} : ", row * 16 + startAddress);
			
			for (auto col = 0; col < 16; col++)
			{
				str += std::format("{:02x} ", m_Data[row * 16 + startAddress + col]);
			}

			ImGui::Text("%s", str.c_str());
		}

	}


}
