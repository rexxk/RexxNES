#include "emu/ppu/ppu.h"
#include "emu/cpu6502/cpu.h"

#include <chrono>
#include <print>
#include <thread>

using namespace std::chrono_literals;


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



namespace emu
{

	PPU::PPU(Memory& ppuMemory, Memory& cpuMemory)
		: m_PPUMemory(ppuMemory), m_CPUMemory(cpuMemory)
	{
		m_Pixels.resize(256 * 240);
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


			for (std::uint16_t scanline = 0; scanline < 241; scanline++)
			{
				ProcessScanline(scanline);
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

//		for (std::uint16_t col = 0; col < 240; col++)
//		{
//			auto nametableValue = m_PPUMemory.Read(0x2000 + scanline * 240 + col);
//
//			std::println("Nametable value: {:02x}", nametableValue);
//		}


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

}
