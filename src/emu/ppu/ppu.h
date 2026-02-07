#pragma once

#include "emu/memory/memory.h"

#include <array>
#include <atomic>
#include <span>
#include <vector>


namespace emu
{

	class PPU
	{
	public:
		PPU() = delete;
		PPU(Memory& ppuMemory, Memory& cpuMemory, std::uint8_t nametableAlignment);

		static auto ToggleW() -> void;
		static auto ResetW() -> void;

		static auto WritePPUAddress(std::uint8_t value) -> void;
		static auto WritePPUData(std::uint8_t value, std::uint8_t increment) -> void;

		static auto ReadPPUAddress() -> std::uint16_t;
		static auto ReadPPUData(std::uint8_t increment) -> std::uint8_t;

		auto Stop() -> void;

		auto Execute() -> void;

		auto GenerateImageData(std::span<std::uint8_t> imageData) -> void;
		auto GetInternalMemory() -> std::array<std::uint8_t, 0x100>& { return m_OAM; }

	private:
		auto ProcessScanline(std::uint16_t scanline) -> std::uint16_t;

		auto ReadMemory(std::uint16_t address) -> std::uint8_t;
		auto WriteMemory(std::uint16_t address, std::uint8_t value) -> void;

	private:
		Memory& m_CPUMemory;
		Memory& m_PPUMemory;

		std::uint8_t m_NametableAlignment{};

		std::array<std::uint8_t, 0x100> m_OAM{};
		std::vector<std::uint8_t> m_Pixels{};

		std::atomic<bool> m_Executing{ false };
	};

}
