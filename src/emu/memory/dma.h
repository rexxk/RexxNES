#pragma once

#include "emu/memory/memory.h"

#include <array>
#include <cstdint>
#include <span>


namespace emu
{


	class DMA
	{
	public:
		DMA() = delete;
		DMA(std::uint16_t address, Memory& src, std::span<std::uint8_t> dst);

		auto Execute() -> std::uint16_t;

	private:
		std::uint16_t m_Address{};

		Memory& m_SourceMemory;
		std::span<std::uint8_t> m_DestinationMemory;
	};


}
