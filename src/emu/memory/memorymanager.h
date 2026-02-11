#pragma once

#include <cstdint>
#include <memory>
#include <span>
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
		MemoryManager();
		~MemoryManager();

		auto AddChunk(MemoryChunk& chunk) -> void { m_Chunks.push_back(chunk); }

	private:
		std::vector<MemoryChunk> m_Chunks{};

	};


}
