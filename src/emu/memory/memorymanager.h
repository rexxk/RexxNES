#pragma once

#include "emu/cartridge/cartridge.h"
#include "emu/memory/ram.h"
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
		ASU,
		Cartridge,
	};

	struct MemoryChunk
	{
		std::uint16_t StartAddress{ 0l };
		std::uint16_t Size{ 0l };

		std::uint8_t ID{ 0l };

		MemoryType Type{ MemoryType::RAM };
		MemoryOwner Owner{ MemoryOwner::CPU };
	};


	class MemoryManager
	{
	public:
		explicit MemoryManager(Cartridge& cartridge);
		~MemoryManager();

		auto AddChunk(MemoryChunk& chunk) -> void;

		auto ReadMemory(MemoryOwner owner, std::uint16_t address) -> std::uint8_t;
		auto WriteMemory(MemoryOwner owner, std::uint16_t address, std::uint8_t value) -> void;
		auto GetIOAddress(std::uint16_t address) -> std::uint8_t&;

		auto DMATransfer(MemoryOwner targetOwner) -> void;

		auto ViewMemory() -> void;

	private:
		Cartridge& m_Cartridge;

		std::vector<MemoryChunk> m_Chunks{};

		std::unordered_map<std::uint8_t, RAM> m_RAMs{};
	};


}
