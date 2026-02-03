#include "emu/ppu/ppu.h"
#include "emu/cpu6502/cpu.h"
#include "emu/memory/memory.h"

#include <chrono>
#include <cstdint>
#include <print>
#include <thread>


using namespace std::chrono_literals;

namespace emu
{

	static constexpr std::uint16_t PPUCTRL = 0x2000;
	static constexpr std::uint16_t PPUMASK = 0x2001;
	static constexpr std::uint16_t PPUSTATUS = 0x2002;
	static constexpr std::uint16_t OAMADDR = 0x2003;
	static constexpr std::uint16_t OAMDATA = 0x2004;
	static constexpr std::uint16_t PPUSCROLL = 0x2005;
	static constexpr std::uint16_t PPUADDR = 0x2006;
	static constexpr std::uint16_t PPUDATA = 0x2007;
	static constexpr std::uint16_t OAMDMA = 0x4014;
	
	static std::uint16_t RegV{};
	static std::uint16_t RegT{};
	static std::uint8_t RegX{};
	static std::uint8_t RegW{};
	
	
	static Memory PPU_CIRAM{ 2 };
	static std::uint16_t PPUAddress{};



	PPU::PPU(Memory& ppuMemory, Memory& cpuMemory, std::uint8_t nametableAlignment)
		: m_PPUMemory(ppuMemory), m_CPUMemory(cpuMemory), m_NametableAlignment(nametableAlignment)
	{
		m_Pixels.resize(256 * 240);
	}


	auto PPU::TriggerPPUAddress(std::uint8_t value) -> void
	{
		if (RegW == 0) PPUAddress = (value << 8) & 0xFF00;
		else if (RegW == 1) PPUAddress |= value;
	}

	auto PPU::TriggerPPUData(std::uint8_t value, std::uint8_t increment) -> void
	{
		PPU_CIRAM.Write(PPUAddress - 0x2000, value);

		PPUAddress += increment;
	}


	auto PPU::Execute() -> void
	{
		std::println("Starting PPU");

		m_Executing.store(true);

		auto framesPerSecond = 60.0988f;
		auto frameTime = 1.0f / framesPerSecond;

		std::println("Frametime: {}s", frameTime);

		while (m_Executing.load())
		{
			// Copy CIRAM to 0x2000 in PPU memory
			{
				std::memset(m_PPUMemory.GetData() + 0x2000, *PPU_CIRAM.GetData(), 0x2000);
			}

			// Clear VBlank flag
			{
				auto value = m_CPUMemory.Read(PPUSTATUS);
				value &= 0x7F;
				m_CPUMemory.Write(PPUSTATUS, value);
			}

			// Do all frame processing
			{
				auto oamAddress = m_CPUMemory.Read(OAMADDR);
			}


//			for (std::uint16_t scanline = 0; scanline < 241; scanline++)
//			{
//				ProcessScanline(scanline);
//			}

			for (std::uint16_t index = 0; index < 32 * 30; index++)
			{
				auto nametableValue = ReadMemory(0x2000 + index);
			}

			for (std::uint16_t index = 0; index < 64; index++)
			{
				auto nametableAttribute = ReadMemory(0x23c0 + index);
			}



			// Set VBlank flag
			{
				auto value = m_CPUMemory.Read(PPUSTATUS);
				value |= 0x80;
				m_CPUMemory.Write(PPUSTATUS, value);
			}

			auto ppuCtrl = m_CPUMemory.Read(PPUCTRL);

			if (ppuCtrl & 0x80)
				CPU::TriggerNMI();

			std::this_thread::sleep_for(16ms);

		}

		std::println("Stopping PPU");
	}

	auto PPU::Stop() -> void
	{
		m_Executing.store(false);
	}


	auto PPU::ProcessScanline(std::uint16_t scanline) -> std::uint16_t
	{
		std::uint16_t cycles{};

		const std::uint16_t nametableBase = 0x2000;

		// 2 bits per pixel - 4 pixels per byte
		// First "cycle" - fetch data
		// Second "cycle" - create pixels

		auto nametableValue = ReadMemory(0x2000);

		// Tile size - 8x8

		// Idea - read all tiles (32x30) and interpolate all 8 pixels per tile and scanline. Make the pixel data from that.

		return cycles;
	}


	auto PPU::ToggleW() -> void
	{
		++RegW &= 0x01;

	}
	
	auto PPU::ResetW() -> void
	{
		RegW = 0;
	}

	auto PPU::ReadMemory(std::uint16_t address) -> std::uint8_t
	{
		return m_PPUMemory.Read(address);
	}

	auto PPU::WriteMemory(std::uint16_t address, std::uint8_t value) -> void
	{
		m_PPUMemory.Write(address, value);
	}

}
