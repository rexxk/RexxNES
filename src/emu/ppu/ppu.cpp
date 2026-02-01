#include "emu/ppu/ppu.h"

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



namespace emu
{

	PPU::PPU(Memory& ppuMemory, Memory& cpuMemory)
		: m_PPUMemory(ppuMemory), m_CPUMemory(cpuMemory)
	{

	}


	auto PPU::Execute() -> void
	{
		std::println("Starting PPU");

		while (m_Executing.load())
		{

			std::this_thread::sleep_for(10ms);
		}

		std::println("Stopping PPU");
	}

	auto PPU::Stop() -> void
	{
		m_Executing.store(false);
	}

}
