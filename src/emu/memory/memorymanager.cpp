#include "emu/memory/memorymanager.h"



namespace emu
{


	MemoryManager::MemoryManager(Cartridge& cartridge)
		: m_Cartridge(cartridge)
	{

	}

	MemoryManager::~MemoryManager()
	{

	}


	auto MemoryManager::AddChunk(MemoryChunk& chunk) -> void 
	{ 
		chunk.ID = static_cast<std::uint8_t>(m_Chunks.size()) + 1;
		m_Chunks.push_back(chunk); 

		if (chunk.Type == MemoryType::RAM || chunk.Type == MemoryType::IO)
			m_RAMs[chunk.ID] = RAM{ chunk.Size };
	}


	auto MemoryManager::ReadMemory(MemoryOwner owner, std::uint16_t address) -> std::uint8_t
	{
		for (auto& chunk : m_Chunks)
		{
			if (address >= chunk.StartAddress && address < chunk.StartAddress + chunk.Size)
			{
				if (chunk.Type == MemoryType::ROM && owner == MemoryOwner::CPU)
					return m_Cartridge.GetROM(ROMType::Program).ReadAddress(address - chunk.StartAddress);
				else if (chunk.Type == MemoryType::ROM && owner == MemoryOwner::PPU)
					return m_Cartridge.GetROM(ROMType::Character).ReadAddress(address - chunk.StartAddress);

				if (chunk.Type == MemoryType::IO)
					return m_RAMs[chunk.ID].ReadAddress(address - chunk.StartAddress);
				else if (chunk.Type == MemoryType::RAM && owner == chunk.Owner)
					return m_RAMs[chunk.ID].ReadAddress(address - chunk.StartAddress);
			}
		}

		return 0;
	}

	auto MemoryManager::WriteMemory(MemoryOwner owner, std::uint16_t address, std::uint8_t value) -> void
	{
		for (auto& chunk : m_Chunks)
		{
			if (chunk.Type == MemoryType::RAM)
			{
				if (address >= chunk.StartAddress && address < chunk.StartAddress + chunk.Size && owner == chunk.Owner)
				{
					m_RAMs[chunk.ID].WriteAddress(address - chunk.StartAddress, value);
				}
			}
			else if (chunk.Type == MemoryType::IO)
			{
				if (address >= chunk.StartAddress && address < chunk.StartAddress + chunk.Size)
				{
					m_RAMs[chunk.ID].WriteAddress(address - chunk.StartAddress, value);
				}
			}
		}
	}

}
