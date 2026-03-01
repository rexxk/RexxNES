#pragma once

#include "emu/cartridge/cartridge.h"

#include <cstdint>
#include <string>


namespace emu
{

	struct Memory
	{
		std::vector<std::uint8_t> Data;
		std::uint16_t StartAddress{ 0u };
		std::uint16_t Size{ 0u };
		bool ReadOnly{ false };

		std::string Name{};
	};

	struct MemoryMap
	{
		Memory ProgramROM;
		Memory CPURAM;

		Memory CharROM;
		Memory PPURAM;

		Memory OAMRAM;
		Memory APURAM;

		Memory PPUIO;
		Memory APUIO;

		std::uint8_t NametableMirroring{ 0u };
	};

	class Mapper
	{
	public:
		static auto CreateMemoryMap(Cartridge& cartridge) -> MemoryMap;
	};


}
