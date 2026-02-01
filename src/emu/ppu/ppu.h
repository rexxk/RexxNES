#pragma once

#include "emu/memory/memory.h"

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

	private:
		Memory& m_PPUMemory;
		Memory& m_CPUMemory;

		std::atomic<bool> m_Executing{ false };
	};

}
