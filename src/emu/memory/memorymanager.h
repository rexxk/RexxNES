#pragma once

#include "emu/cartridge/cartridge.h"
#include "emu/memory/ram.h"
#include "emu/memory/rom.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>


namespace emu
{

	enum class MemoryOwner
	{
		CPU,
		PPU,
		ASU,
		Cartridge,
	};


	class MemoryManager
	{
	public:
		explicit MemoryManager(Cartridge& cartridge);
		~MemoryManager();

		auto ReadCharROM(std::uint16_t address) -> std::uint8_t;
		auto ReadProgramROM(std::uint16_t address) -> std::uint8_t;

		auto ReadAPURAM(std::uint16_t address) -> std::uint8_t;
		auto ReadCPURAM(std::uint16_t address) -> std::uint8_t;
		auto ReadOAMRAM(std::uint16_t address) -> std::uint8_t;
		auto ReadPPURAM(std::uint16_t address) -> std::uint8_t;

		auto ReadAPUIO(std::uint16_t address) -> std::uint8_t;
		auto ReadPPUIO(std::uint16_t address) -> std::uint8_t;

		auto WriteAPURAM(std::uint16_t address, std::uint8_t value) -> void;
		auto WriteCPURAM(std::uint16_t address, std::uint8_t value) -> void;
		auto WriteOAMRAM(std::uint16_t address, std::uint8_t value) -> void;
		auto WritePPURAM(std::uint16_t address, std::uint8_t value) -> void;

		auto WriteAPUIO(std::uint16_t address, std::uint8_t value) -> void;
		auto WritePPUIO(std::uint16_t address, std::uint8_t value) -> void;

		auto DMATransfer(MemoryOwner targetOwner, std::uint8_t value) -> void;

		auto GetScrollXRegister() const -> const std::uint16_t;
		auto GetScrollYRegister() const -> const std::uint16_t;

		auto ViewMemory() -> void;

	private:
		Cartridge& m_Cartridge;
	};


}
