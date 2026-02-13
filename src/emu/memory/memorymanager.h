#pragma once

#include "emu/cartridge/cartridge.h"
#include "emu/memory/rom.h"

#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>


namespace emu
{


	enum class MemoryType
	{
		ROM,
		RAM,
		IO,
	};

	enum class MemoryOwner
	{
		CPU,
		PPU,
		Cartridge,
	};

	struct MemoryChunk
	{
		std::uint16_t StartAddress{ 0l };
		std::uint16_t Size{ 0l };

		MemoryType Type{ MemoryType::RAM };
		MemoryOwner Owner{ MemoryOwner::CPU };
	};


	class MemoryManager
	{
	public:
		explicit MemoryManager(Cartridge& cartridge);
		~MemoryManager();

		auto AddChunk(MemoryChunk& chunk) -> void { m_Chunks.push_back(chunk); }


	private:
		Cartridge& m_Cartridge;

		std::vector<MemoryChunk> m_Chunks{};

		std::unordered_map<ROMType, ROM> m_ROMs{};
	};


}
