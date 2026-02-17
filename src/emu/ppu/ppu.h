#pragma once

#include "emu/memory/memorymanager.h"
#include "emu/system/powerhandler.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <span>
#include <vector>


namespace emu
{

	class PPU
	{
	public:
		PPU() = delete;
		PPU(PowerHandler& powerHandler, MemoryManager& memoryManager, std::uint8_t nametableAlignment);

		static auto ToggleW() -> void;
		static auto ResetW() -> void;

		auto Stop() -> void;

		auto UpdatePowerState() -> void;

		auto Execute() -> void;

		auto GenerateImageData(std::span<std::uint8_t> imageData) -> void;
		auto GetInternalMemory() -> std::array<std::uint8_t, 0x100>& { return m_OAM; }
		auto GetImageData() -> std::vector<std::uint8_t>& { return m_ImageData; }

	private:
		auto ProcessScanline(std::uint16_t scanline) -> std::uint16_t;

		auto ReadMemory(std::uint16_t address) -> std::uint8_t;
		auto WriteMemory(std::uint16_t address, std::uint8_t value) -> void;

	private:
		MemoryManager& m_MemoryManager;
		PowerHandler& m_PowerHandler;

		std::uint8_t m_NametableAlignment{};

		std::array<std::uint8_t, 0x100> m_OAM{};
		std::vector<std::uint8_t> m_Pixels{};

		std::span<std::uint8_t> m_MMIO;

		std::atomic<bool> m_Executing{ false };

		std::condition_variable m_CV{};
		std::mutex m_Mutex{};

		std::vector<std::uint8_t> m_ImageData;

	};

}
