#pragma once

#include "emu/memory/memory.h"

#include <array>
#include <atomic>
#include <vector>

namespace emu
{

	class PPU
	{
	public:
		PPU() = delete;
		PPU(Memory& ppuMemory, Memory& cpuMemory);

		static auto ToggleW() -> void;
		static auto ResetW() -> void;

		auto Stop() -> void;

		auto Execute() -> void;

		auto GetInternalMemory() -> std::array<std::uint8_t, 0x100>& { return m_OAM; }

	private:
		auto ProcessScanline(std::uint16_t scanline) -> std::uint16_t;

	private:
		Memory& m_PPUMemory;
		Memory& m_CPUMemory;

		std::array<std::uint8_t, 0x100> m_OAM{};
		std::vector<std::uint8_t> m_Pixels{};

		std::atomic<bool> m_Executing{ false };
	};

}
