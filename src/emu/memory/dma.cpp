#include "emu/memory/dma.h"

#include <print>


namespace emu
{

	DMA::DMA(std::uint16_t address, Memory& srcMemory, std::span<std::uint8_t> dstMemory)
		: m_Address(address), m_SourceMemory(srcMemory), m_DestinationMemory(dstMemory)
	{

	}

	auto DMA::Execute() -> std::uint16_t
	{
		auto address = m_SourceMemory.Read(m_Address);
		std::uint16_t sourceAddress = ((address << 8) & 0xFF00);

		for (std::uint16_t index = 0; index < 0x100; index++)
		{
			m_DestinationMemory[index] = m_SourceMemory.Read(sourceAddress + index);
		}

//		std::println("DMA data transfer: {:04x} ({:04x})", sourceAddress, m_Address);

		// Operation takes 513 or 514 cycles depending on alignment need
		return 514;
	}

}
