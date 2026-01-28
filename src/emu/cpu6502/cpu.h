#pragma once

#include "emu/memory/memory.h"

#include <cstdint>
#include <span>
#include <string_view>


namespace emu
{

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
		explicit CPU(Memory& memory);

		auto GetRegisters() -> Registers&;
		auto GetFlags() -> const std::uint8_t;

		auto Execute(std::uint16_t startVector = 0) -> void;
//		auto Execute(std::span<std::uint8_t> program, const std::uint16_t memoryLocation) -> void;

		auto ReadAddress(std::uint16_t address) -> std::uint8_t;
		auto WriteAddress(std::uint16_t address, std::uint8_t value) -> void;

		auto FetchAbsluteAddressRegister(std::uint8_t Registers::* reg) -> std::uint16_t;
		auto FetchAddress() -> std::uint16_t;
		auto FetchZeropageAddress() -> std::uint16_t;
		auto ReadAbsoluteAddress() -> std::uint8_t;
		auto ReadAbsoluteAddressRegister(std::uint8_t Registers::* reg, std::string_view regString) -> std::uint8_t;
		auto ReadIndirectIndexed() -> std::uint8_t;
		auto ReadZeropageAddress() -> std::uint8_t;
		auto WriteAbsoluteAddress(const std::uint8_t value) -> void;
		auto WriteAbsoluteAddressRegister(std::uint8_t Registers::* reg, const std::uint8_t value, std::string_view regString) -> void;
		auto WriteIndirectIndexed(std::uint8_t value) -> void;
		auto WriteZeropageAddress(const std::uint8_t value) -> void;

		//		auto AbsoluteAddress() -> uint16_t;

	private:
		Memory& m_Memory;

		std::uint16_t m_StartVector{ 0 };

	};


}
