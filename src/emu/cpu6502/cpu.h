#pragma once

#include "emu/memory/memorymanager.h"
#include "emu/system/powerhandler.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <span>
#include <string_view>


namespace emu
{

	enum class RunningMode
	{
		Halt,
		Run,
		Step,
	};

	struct Registers
	{
		std::uint8_t A{};
		std::uint8_t X{};
		std::uint8_t Y{};
		std::uint16_t PC{ 0xFFFC };
		std::uint8_t SP{ 0xFD };
	};

	class CPU
	{
	public:
		CPU() = delete;
//		explicit CPU(MemoryManager& memoryManager, DMA& oamDMA, DMA& dmcDMA);
		explicit CPU(PowerHandler& powerHandler, MemoryManager& memoryManager);

		auto Stop() -> void;

		static auto TriggerNMI() -> void;
		static auto NMIRunning() -> bool;
		auto StepToRTS() -> void;

		auto UpdatePowerState() -> void;

		auto GetRegisters() -> Registers&;
		auto GetFlags() -> const std::uint8_t;

		auto Execute(std::uint16_t startVector = 0) -> void;
//		auto Execute(std::span<std::uint8_t> program, const std::uint16_t memoryLocation) -> void;

		inline auto ReadAddress(std::uint16_t address) -> std::uint8_t;
		inline auto WriteAddress(std::uint16_t address, std::uint8_t value) -> void;

		inline auto FetchAbsoluteAddress() -> std::uint16_t;
		inline auto FetchAbsluteAddressRegister(std::uint8_t Registers::* reg) -> std::uint16_t;
		inline auto FetchIndirectIndexedAddress() -> std::uint16_t;
		inline auto FetchZeropageAddress() -> std::uint16_t;
		inline auto FetchZeropageAddressRegister(std::uint8_t Registers::*reg) -> std::uint16_t;
		inline auto ReadAbsoluteAddress() -> std::uint8_t;
		inline auto ReadAbsoluteAddressRegister(std::uint8_t Registers::* reg) -> std::uint8_t;
		inline auto ReadIndirectIndexed() -> std::uint8_t;
		inline auto ReadZeropageAddress() -> std::uint8_t;
		inline auto ReadZeropageAddressRegister(std::uint8_t Registers::* reg) -> std::uint8_t;
		inline auto WriteAbsoluteAddress(const std::uint8_t value) -> void;
		inline auto WriteAbsoluteAddressRegister(std::uint8_t Registers::* reg, const std::uint8_t value) -> void;
		inline auto WriteIndirectIndexed(std::uint8_t value) -> void;
		inline auto WriteZeropageAddress(const std::uint8_t value) -> void;
		inline auto WriteZeropageAddressRegister(std::uint8_t Registers::* reg, const std::uint8_t value) -> void;

		//		auto AbsoluteAddress() -> uint16_t;

	private:
		MemoryManager& m_MemoryManager;
		PowerHandler& m_PowerHandler;

		std::uint16_t m_StartVector{ 0 };

		std::atomic<bool> m_Executing{ false };

		std::condition_variable m_CV{};
		std::mutex m_Mutex{};
	};


}
