#pragma once

#include "emu/memory/memory.h"

#include <array>
#include <atomic>

namespace emu
{

	class PPU
	{
	public:
		PPU() = delete;
		PPU(Memory& ppuMemory, Memory& cpuMemory);

		auto Stop() -> void;

		auto Execute() -> void;

		auto GetInternalMemory() -> std::array<std::uint8_t, 0x100>& { return m_OAM; }

	private:
		Memory& m_PPUMemory;
		Memory& m_CPUMemory;

		std::array<std::uint8_t, 0x100> m_OAM{};

		std::atomic<bool> m_Executing{ false };
	};

}
