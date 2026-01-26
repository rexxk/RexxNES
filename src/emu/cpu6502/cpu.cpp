#include "emu/cpu6502/cpu.h"

#include <bitset>
#include <chrono>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>


using namespace std::chrono_literals;

namespace emu
{

#define PRINT_COMMANDS	0

	
	constexpr std::uint8_t FlagCarry = 0;
	constexpr std::uint8_t FlagZero = 1;
	constexpr std::uint8_t FlagInterrupt = 2;
	constexpr std::uint8_t FlagDecimal = 3;
	constexpr std::uint8_t FlagBreak = 4;
	constexpr std::uint8_t FlagOverflow = 6;
	constexpr std::uint8_t FlagNegative = 7;

	constexpr std::uint16_t StackLocation = 0x0100;

	static Registers s_Registers{};
	static std::bitset<8> s_Flags{};


	struct OpValue
	{
		std::uint8_t Size{ 0 };
		std::uint8_t ClockCycles{ 0 };
	};

	static auto PrintRegisters() -> void
	{
		std::println("Flags:\n        NVxB DIZC");
		std::print("        ");

		s_Flags[FlagNegative] ? std::print("x") : std::print("-");
		s_Flags[FlagOverflow] ? std::print("x-") : std::print("--");
		s_Flags[FlagBreak] ? std::print("x ") : std::print("- ");
		s_Flags[FlagDecimal] ? std::print("x") : std::print("-");
		s_Flags[FlagInterrupt] ? std::print("x") : std::print("-");
		s_Flags[FlagZero] ? std::print("x") : std::print("-");
		s_Flags[FlagCarry] ? std::print("x") : std::print("-");

		std::println("\nRegisters:\n        A : {:02x}", s_Registers.A);
		std::println("        X : {:02x}", s_Registers.X);
		std::println("        Y : {:02x}", s_Registers.Y);
		std::println("        PC: {:04x}", s_Registers.PC);
		std::println("        SP: {:02x}", s_Registers.SP);
	}

	static auto PrintCommand(std::string_view cmd) -> void
	{
#if PRINT_COMMANDS
		std::print("@ {:04x}   {}", s_Registers.PC, cmd);
#endif
	}

	static auto PrintCommandArg(std::uint16_t value, bool immediate = false, bool decimal = false) -> void
	{
#if PRINT_COMMANDS
		if (immediate)
			std::print("   #");
		else
			std::print("   ");

		if (decimal)
			std::println("${}", static_cast<std::int16_t>(value));
		else
			std::println("${:02x}", value);
#endif
	}

	static auto PrintCommandArgAbsolute(std::uint16_t value, std::string_view reg) -> void
	{
#if PRINT_COMMANDS
		std::println("   ${:04x},{}", value, reg);
#endif
	}

	static auto PrintCommandArgIndirect(std::uint8_t value, std::string_view reg) -> void
	{
#if PRINT_COMMANDS
		std::println("   (${:02x}),{}", value, reg);
#endif
	}

	static auto PrintSingleCommand(std::string_view cmd) -> void
	{
#if PRINT_COMMANDS
		std::println("@ {:04x}   {}", s_Registers.PC, cmd);
#endif
	}

#if 0
	static auto Bit(std::uint8_t value) -> void
	{
		s_Flags[FlagZero] = s_Registers.A & value;
		s_Flags[FlagOverflow] = value & 0b0100'0000;
		s_Flags[FlagNegative] = value & 0b1000'0000;
	}

	static auto BitAbsolute(CPU& cpu) -> std::optional<OpValue>
	{
		Bit(cpu.ReadAbsoluteAddress());

		return OpValue{ 3, 4 };
	}

	static auto BitZeropage(CPU& cpu) -> std::optional<OpValue>
	{
//		auto zeropageAddress = cpu.ReadAddress(s_Registers.PC + 1);
//		auto operand = cpu.ReadAddress(zeropageAddress);
		Bit(cpu.ReadZeropageAddress());
//		Bit(operand);

		return OpValue{ 2, 3 };
	}

	static auto Branch(CPU& cpu, const std::uint8_t flag, bool condition) -> std::optional<OpValue>
	{
		std::int8_t relativePosition = cpu.ReadAddress(s_Registers.PC + 1);

		bool boundaryCrossed = ((s_Registers.PC + relativePosition) & 0xFF00) != (s_Registers.PC & 0xFF00);

		if (s_Flags[flag] == condition)
		{
			s_Registers.PC += relativePosition;
//			return OpValue{ 0, static_cast<std::uint8_t>(2 + (boundaryCrossed ? 2 : 1)) };
		}
	
		return OpValue{ 2, static_cast<std::uint8_t>(2 + (boundaryCrossed ? 2 : 1)) };
	}


	static auto CmpImmediate(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		auto value = cpu.ReadAddress(s_Registers.PC + 1);

		auto result = s_Registers.*reg - value;
		s_Flags[FlagNegative] = result & 0b1000'0000;
		s_Flags[FlagZero] = result == 0;
		s_Flags[FlagCarry] = static_cast<int8_t>(result) >= 0;

		return OpValue{ 2, 2 };
	}

	static auto Dec(std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		s_Registers.*reg = s_Registers.*reg - 1;

		s_Flags[FlagNegative] = s_Registers.*reg & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.*reg == 0;

		return OpValue{ 1, 2 };
	}

	static auto Inc(std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		s_Registers.*reg = s_Registers.*reg + 1;

		s_Flags[FlagNegative] = s_Registers.*reg & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.*reg == 0;

		return OpValue{ 1, 2 };
	}

	static auto JmpAbsolute(CPU& cpu) -> std::optional<OpValue>
	{
		cpu.WriteAddress(StackLocation + s_Registers.SP--, static_cast<std::uint8_t>((s_Registers.PC + 3) & 0xFF));
		cpu.WriteAddress(StackLocation + s_Registers.SP--, static_cast<std::uint8_t>(((s_Registers.PC + 3) & 0xFF00) >> 8));

		auto addressLow = cpu.ReadAddress(s_Registers.PC + 1);
		auto addressHigh = cpu.ReadAddress(s_Registers.PC + 2);
		std::uint16_t address = (addressHigh << 8) + addressLow;

		s_Registers.PC = address;

		return OpValue{ 0, 3 };
	}

	static auto ReturnFromSubroutine(CPU& cpu) -> std::optional<OpValue>
	{
		auto addressHigh = cpu.ReadAddress(StackLocation + ++s_Registers.SP);
		auto addressLow = cpu.ReadAddress(StackLocation + ++s_Registers.SP);
		std::uint16_t address = (addressHigh << 8) + addressLow;

		s_Registers.PC = address;

		return OpValue{ 0, 6 };
	}

	static auto LdAbsolute(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		auto value = cpu.ReadAbsoluteAddress();
		s_Registers.*reg = value;

		s_Flags[FlagNegative] = value & 0b1000'0000;
		s_Flags[FlagZero] = value == 0;

		return OpValue{ 3, 4 };
	}

	static auto LdAbsoluteReg(CPU& cpu, std::uint8_t Registers::* reg, std::uint8_t Registers::* offsetReg, std::string_view regString) -> std::optional<OpValue>
	{
		auto value = cpu.ReadAbsoluteAddressRegister(offsetReg);

		s_Registers.*reg = value;

		s_Flags[FlagNegative] = value & 0b1000'0000;
		s_Flags[FlagZero] = value == 0;

		bool boundaryCrossed = (((s_Registers.PC + s_Registers.*offsetReg) & 0xFF00) != (s_Registers.PC & 0xFF00));

		return OpValue{ 3, static_cast<std::uint8_t>(4 + boundaryCrossed ? 1 : 0) };
	}

	static auto LdImmediate(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		auto value = cpu.ReadAddress(s_Registers.PC + 1);

		s_Registers.*reg = value;

		s_Flags[FlagNegative] = value & 0b1000'0000;
		s_Flags[FlagZero] = value == 0;

		return OpValue{ 2, 2 };
	}

	static auto StAbsolute(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		cpu.WriteAbsoluteAddress(s_Registers.*reg);

		return OpValue{ 3, 4 };
	}

	static auto StZeropage(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		cpu.WriteZeropageAddress(s_Registers.*reg);

		return OpValue{ 2, 3 };
	}

	static auto StaAbsoluteReg(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		cpu.WriteAbsoluteAddressRegister(reg, s_Registers.A);

		return OpValue{ 3, 5 };
	}

	static auto StaIndirectIndexed(CPU& cpu) -> std::optional<OpValue>
	{
		cpu.WriteIndirectIndexed(s_Registers.A);

		return OpValue{ 2, 6 };
	}

	static auto Transfer(std::uint8_t Registers::* from, std::uint8_t Registers::* to) -> std::optional<OpValue>
	{
		s_Registers.*to = s_Registers.*from;

		s_Flags[FlagNegative] = s_Registers.*to & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.*to == 0;

		return OpValue{ 1, 2 };
	}

	static auto ExecuteOpcode(CPU& cpu, std::uint8_t opCode) -> const std::optional<OpValue>
	{
		switch (opCode)
		{
//			// BPL (BPL #rel) - branch if N=0 -
//			case 0x10:
//			{
//				return Branch(cpu, FlagNegative, false);
//			}
//
			// JMP (JMP $xxxx) -
//			case 0x20:
//			{
//				std::println("JUMP!");
//				return JmpAbsolute(cpu);
//			}

			// BIT zeropage (BIT $xx) - NVZ
//			case 0x24:
//			{
//				return BitZeropage(cpu);
//			}
//
//			// BIT absolute (BIT #$xx) - NVZ
//			case 0x2C:
//			{
//				return BitAbsolute(cpu);
//			}

			// RTS
//			case 0x60:
//			{
//				std::println("RTS!");
//				return ReturnFromSubroutine(cpu);
//			}

			// SEI - 1-I
//			case 0x78:
//			{
//				s_Flags[FlagInterrupt] = true;
//				return OpValue{ 1, 2 };
//			}

			// STA zeropage (STA $xx) - A->M  -
//			case 0x85:
//			{
//				return StZeropage(cpu, &Registers::A);
//			}

			// STX zeropage (STX $xx) - X->M -
//			case 0x86:
//			{
//				return StZeropage(cpu, &Registers::X);
//			}

			// DEY - Y-1->Y NZ
//			case 0x88:
//			{
//				return Dec(&Registers::Y);
//			}

			// STA absolute (STA $xxxx) - A->M -
//			case 0x8D:
//			{
//				return StAbsolute(cpu, &Registers::A);
//			}

			// STA indirect (STA ($xx),Y) - A->M -
//			case 0x91:
//			{
//				return StaIndirectIndexed(cpu);
//			}

			// STA abs, Y (STA #$xx,Y) -
//			case 0x99:
//			{
//				return StaAbsoluteReg(cpu, &Registers::Y);
//			}

			// TXS - X->SP -
//			case 0x9A:
//			{
//				s_Registers.SP = s_Registers.X;
//				return OpValue{ 1, 2 };
//			}

			// LDX immediate (LDX #xxxx) - M->X NZ
//			case 0xA2:
//			{
//				return LdImmediate(cpu, &Registers::X);
//			}
//
//			// LDA immediate (LDA #$xx) - M->A  NZ
//			case 0xA9:
//			{
//				return LdImmediate(cpu, &Registers::A);
//			}
	
			// LDY immediate (LDY #$xx) - M->Y NZ
//			case 0xA0:
//			{
//				return LdImmediate(cpu, &Registers::Y);
//			}

			// TAX - A->X NZ
//			case 0xAA:
//			{
//				return Transfer(&Registers::A, &Registers::X);
//			}

			// LDA absolute (LDA $xx) - M->A NZ
//			case 0xAD:
//			{
//				return LdAbsolute(cpu, &Registers::A);
//			}

			// BCS (BCS #rel) - branch if C=1 -
//			case 0xB0:
//			{
//				return Branch(cpu, FlagCarry, true);
//			}

			// LDA absolute X (LDA $xx,X) - M->A NZ
//			case 0xBD:
//			{
//				return LdAbsoluteReg(cpu, &Registers::A, &Registers::X, "X");
//			}

			// CPY immediate (CPY #$xx) - A-M  NZC
//			case 0xC0:
//			{
//				return CmpImmediate(cpu, &Registers::Y);
//			}

			// INY - Y+1->Y NZ
			case 0xC8:
			{
				return Inc(&Registers::Y);
			}

			// CMP immediate (CMP #$xx) - A-M  NZC
			case 0xC9:
			{
				return CmpImmediate(cpu, &Registers::A);
			}

			// DEX implied - X-1->X NZ
//			case 0xCA:
//			{
//				return Dec(&Registers::X);
//			}

			// BNE relative (BNE #xx)
//			case 0xD0:
//			{
//				return Branch(cpu, FlagZero, false);
//			}

			// CLD - 0->D
			case 0xD8:
			{
				s_Flags[FlagDecimal] = false;
				return OpValue{ 1, 2 };
			}

			// CPX immediate (CPX #$xx) - X-M  NZC
			case 0xE0:
			{
				return CmpImmediate(cpu, &Registers::X);
			}

			default:
				std::println("Invalid opcode: {:2x}", opCode);
				return std::nullopt;
	
			}
	
		return std::nullopt;
	}


	auto CPU::Execute() -> void
	{
		s_Flags[FlagInterrupt] = true;

		auto vectorLow = m_Memory.Read(s_Registers.PC);
		auto vectorHigh = m_Memory.Read(s_Registers.PC + 1);
		std::uint16_t resetVector = (vectorHigh << 8) + vectorLow;

		std::println("Reset vector: {:04x}", resetVector);

		s_Registers.PC = resetVector;

		// Sets VBLANK flag - keep it static for debugging until PPU is implemented :-)
		m_Memory.Write(0x2002, 0x80);

		const auto frequency = 1'662'607;
		const auto cycleTime = 1'000'000'000 / frequency;

		LARGE_INTEGER li{};
		if (!QueryPerformanceFrequency(&li))
		{
			std::println("QueryPerformanceFrequency failed");
		}

		auto freq = li.QuadPart; //  / 1'000.0;

		std::println("Queried frequency: {}", freq);

		if (freq < frequency)
			std::println("Too high frequency for QueryPerformanceFrequency");

		auto frequencyDivider = static_cast<double>(freq) / frequency;

		std::println("Frequency divider: {}", frequencyDivider);

		std::uint64_t cycles{ 0 };
//		uint8_t old{};
		while (m_Memory.Read(s_Registers.PC) != 0u)
		{
			LARGE_INTEGER startCount{};
			QueryPerformanceCounter(&startCount);

			auto opCode = m_Memory.Read(s_Registers.PC);
			auto maybeExecuted = ExecuteOpcode(*this, opCode);

			if (!maybeExecuted)
				break;
	
//			if (m_Memory.Read(0x1FF) == 0)
//			{
//				std::println("Empty stack data - last op: {}", old);
//			}
//			old = opCode;
			s_Registers.PC += maybeExecuted->Size;

			cycles += maybeExecuted->ClockCycles;
	
			std::int64_t countsElapsed{};

			// Cycle sync
			do
			{
				LARGE_INTEGER endCount;
				QueryPerformanceCounter(&endCount);

				countsElapsed = endCount.QuadPart - startCount.QuadPart;
			} while (static_cast<double>(countsElapsed) < (maybeExecuted->ClockCycles * frequencyDivider));
		}

		std::println("Cycles: {}", cycles);
		PrintRegisters();
	}

#endif

	static auto Branch(CPU& cpu, const std::uint8_t flag, bool condition) -> std::optional<OpValue>
	{
		std::int8_t relativePosition = cpu.ReadAddress(s_Registers.PC + 1);

		s_Registers.PC += 2;

		bool boundaryCrossed = ((s_Registers.PC + relativePosition) & 0xFF00) != (s_Registers.PC & 0xFF00);

		if (s_Flags[flag] == condition)
		{
			if (relativePosition & 0x80)
				s_Registers.PC -= relativePosition;
			else
				s_Registers.PC += relativePosition;
		}

		return OpValue{ 0, static_cast<std::uint8_t>(2 + (boundaryCrossed ? 2 : 1)) };
	}

	static auto Bit(std::uint8_t value) -> void
	{
		s_Flags[FlagZero] = (s_Registers.A & value) == 0;
		s_Flags[FlagOverflow] = value & 0b0100'0000;
		s_Flags[FlagNegative] = value & 0b1000'0000;
	}

	static auto BitAbsolute(CPU& cpu) -> std::optional<OpValue>
	{
		Bit(cpu.ReadAbsoluteAddress());

		return OpValue{ 3, 4 };
	}

	static auto BitZeropage(CPU& cpu) -> std::optional<OpValue>
	{
		Bit(cpu.ReadZeropageAddress());

		return OpValue{ 2, 3 };
	}

	static auto JmpAbsolute(CPU& cpu) -> std::optional<OpValue>
	{
		cpu.WriteAddress(StackLocation + s_Registers.SP--, static_cast<std::uint8_t>((s_Registers.PC + 3) & 0xFF));
		cpu.WriteAddress(StackLocation + s_Registers.SP--, static_cast<std::uint8_t>(((s_Registers.PC + 3) & 0xFF00) >> 8));

		std::println("JMP from 0x{:04x}", s_Registers.PC + 3);

		auto addressLow = cpu.ReadAddress(s_Registers.PC + 1);
		auto addressHigh = cpu.ReadAddress(s_Registers.PC + 2);
		std::uint16_t address = (addressHigh << 8) + addressLow;

		s_Registers.PC = address;

		return OpValue{ 0, 3 };
	}

	static auto ReturnFromSubroutine(CPU& cpu) -> std::optional<OpValue>
	{
		auto addressHigh = cpu.ReadAddress(StackLocation + ++s_Registers.SP);
		auto addressLow = cpu.ReadAddress(StackLocation + ++s_Registers.SP);
		std::uint16_t address = (addressHigh << 8) + addressLow;

		std::println("RTS to 0x{:04x}", address);
		s_Registers.PC = address;

		return OpValue{ 0, 6 };
	}


	CPU::CPU(Memory& memory)
		: m_Memory(memory)
	{

	}

	auto CPU::ReadAddress(std::uint16_t address) -> std::uint8_t
	{
		return m_Memory.Read(address);
	}

	auto CPU::WriteAddress(std::uint16_t address, std::uint8_t value) -> void
	{
		return m_Memory.Write(address, value);
	}


	auto CPU::ReadAbsoluteAddress() -> std::uint8_t
	{
		auto memoryLow = m_Memory.Read(s_Registers.PC + 1);
		auto memoryHigh = m_Memory.Read(s_Registers.PC + 2);
		std::uint16_t address = (memoryHigh << 8) + memoryLow;

		return m_Memory.Read(address);
	}

	auto CPU::ReadAbsoluteAddressRegister(std::uint8_t Registers::* reg) -> std::uint8_t
	{
		auto memoryLow = m_Memory.Read(s_Registers.PC + 1);
		auto memoryHigh = m_Memory.Read(s_Registers.PC + 2);
		std::uint16_t address = (memoryHigh << 8) + memoryLow + s_Registers.*reg;

		return m_Memory.Read(address);
	}

	auto CPU::ReadZeropageAddress() -> std::uint8_t
	{
		auto addressLow = m_Memory.Read(s_Registers.PC + 1);
		std::uint8_t addressHigh{ 0 };
		std::uint16_t address = (addressHigh << 8) + addressLow;

		return m_Memory.Read(address);
	}

	auto CPU::WriteAbsoluteAddress(const std::uint8_t value) -> void
	{
		auto memoryLow = m_Memory.Read(s_Registers.PC + 1);
		auto memoryHigh = m_Memory.Read(s_Registers.PC + 2);
		std::uint16_t address = (memoryHigh << 8) + memoryLow;

		m_Memory.Write(address, value);
	}

	auto CPU::WriteAbsoluteAddressRegister(std::uint8_t Registers::* reg, const std::uint8_t value) -> void
	{
		auto memoryLow = m_Memory.Read(s_Registers.PC + 1);
		auto memoryHigh = m_Memory.Read(s_Registers.PC + 2);
		std::uint16_t address = (memoryHigh << 8) + memoryLow + s_Registers.*reg;

		m_Memory.Write(address, value);
	}

	auto CPU::WriteIndirectIndexed(std::uint8_t value) -> void
	{
		auto zeropageAddress = m_Memory.Read(s_Registers.PC + 1);

		auto addressLow = m_Memory.Read(zeropageAddress);
		auto addressHigh = m_Memory.Read(zeropageAddress + 1);
		std::uint16_t address = (addressHigh << 8) + addressLow;

		address += s_Registers.Y;

		m_Memory.Write(address, value);
	}

	auto CPU::WriteZeropageAddress(const std::uint8_t value) -> void
	{
		auto addressLow = m_Memory.Read(s_Registers.PC + 1);
		std::uint8_t addressHigh{ 0 };
		std::uint16_t address = (addressHigh << 8) + addressLow;

		m_Memory.Write(address, value);
	}


/////////////////////////////////////////////////////////////////////////
//
// OLD
//
/////////////////////////////////////////////////////////////////////////

//	static auto BitAbsolute(CPU& cpu) -> std::optional<OpValue>
//	{
//		std::uint8_t operand = cpu.ReadAddress(s_Registers.PC + 1);
//
//		s_Flags[FlagZero] = s_Registers.A & operand;
//		s_Flags[FlagOverflow] = operand & 0b0100'0000;
//		s_Flags[FlagNegative] = operand & 0b1000'0000;
//
//
//		return OpValue{ 3, 4 };
//	}
//
//	static auto BitZeropage(CPU& cpu) -> std::optional<OpValue>
//	{
//		std::uint8_t address = cpu.ReadAddress(s_Registers.PC + 1);
//		std::uint8_t operand = cpu.ReadAddress(address);
//
//		s_Flags[FlagZero] = s_Registers.A & operand;
//		s_Flags[FlagOverflow] = operand & 0b0100'0000;
//		s_Flags[FlagNegative] = operand & 0b1000'0000;
//
//
//		return OpValue{ 2, 3 };
//	}

//	static auto CmpImmediate(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
//	{
//		auto value = cpu.ReadAddress(s_Registers.PC + 1);
//		PrintCommandArg(value, true);
//
//		auto result = s_Registers.*reg - value;
//		s_Flags[FlagNegative] = result & 0b1000'0000;
//		s_Flags[FlagZero] = result == 0;
//		s_Flags[FlagCarry] = static_cast<int8_t>(result) >= 0;
//
//		return OpValue{ 2, 2 };
//	}

	static auto CmpImmediate(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		auto value = cpu.ReadAddress(s_Registers.PC + 1);

		std::int8_t result = s_Registers.*reg - value;
		s_Flags[FlagNegative] = result & 0b1000'0000;
		s_Flags[FlagZero] = result == 0;
		s_Flags[FlagCarry] = result >= 0;

		return OpValue{ 2, 2 };
	}

//	static auto DeImplied(std::uint8_t Registers::* reg) -> std::optional<OpValue>
//	{
//		s_Registers.*reg = s_Registers.*reg - 1;
//
//		s_Flags[FlagNegative] = s_Registers.*reg & 0b1000'0000;
//		s_Flags[FlagZero] = s_Registers.*reg == 0;
//
//		return OpValue{ 1, 2 };
//	}

	static auto Dec(std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		s_Registers.*reg = s_Registers.*reg - 1;

		s_Flags[FlagNegative] = s_Registers.*reg & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.*reg == 0;

		return OpValue{ 1, 2 };
	}

	static auto Inc(std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		s_Registers.*reg = s_Registers.*reg + 1;

		s_Flags[FlagNegative] = s_Registers.*reg & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.*reg == 0;

		return OpValue{ 1, 2 };
	}

//	static auto InImplied(std::uint8_t Registers::* reg) -> std::optional<OpValue>
//	{
//		s_Registers.*reg = s_Registers.*reg + 1;
//
//		s_Flags[FlagNegative] = s_Registers.*reg & 0b1000'0000;
//		s_Flags[FlagZero] = s_Registers.*reg == 0;
//
//		return OpValue{ 1, 2 };
//	}

//	static auto JmpAbsolute(CPU& cpu) -> std::optional<OpValue>
//	{
//		// Save PC+2 to stack
//		cpu.WriteAddress(StackLocation + s_Registers.SP--, static_cast<std::uint8_t>((s_Registers.PC + 3) & 0xFF));
//		cpu.WriteAddress(StackLocation + s_Registers.SP--, static_cast<std::uint8_t>(((s_Registers.PC + 3) & 0xFF00) >> 8));
//	
//		std::println("JMP from 0x{:04x}", s_Registers.PC + 3);
//	
//		auto addressLow = cpu.ReadAddress(s_Registers.PC + 1);
//		auto addressHigh = cpu.ReadAddress(s_Registers.PC + 2);
//		std::uint16_t address = (addressHigh << 8) + addressLow;
//		PrintCommandArg(address);
//	
//		s_Registers.PC = address;
//	
//		return OpValue{ 0, 3 };
//	}


//	static auto ReturnFromSubroutine(CPU& cpu) -> std::optional<OpValue>
//	{
//		auto addressHigh = cpu.ReadAddress(StackLocation + ++s_Registers.SP);
//		auto addressLow = cpu.ReadAddress(StackLocation + ++s_Registers.SP);
//		std::uint16_t address = (addressHigh << 8) + addressLow;
//
//		std::println("RTS to 0x{:04x}", address);
//
//		s_Registers.PC = address;
//
//		return OpValue{ 0, 6 };
//	}
//

//	static auto LdAbsolute(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
//	{
//		auto addressLow = cpu.ReadAddress(s_Registers.PC + 1);
//		auto addressHigh = cpu.ReadAddress(s_Registers.PC + 2);
//		std::uint16_t address = (addressHigh << 8) + addressLow;
//		PrintCommandArg(address);
//
//		s_Registers.*reg = cpu.ReadAddress(address);
//
//		s_Flags[FlagNegative] = s_Registers.*reg & 0b1000'0000;
//		s_Flags[FlagZero] = s_Registers.*reg == 0;
//
//		return OpValue{ 3, 4 };
//	}

	static auto LdAbsolute(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		auto value = cpu.ReadAbsoluteAddress();
		s_Registers.*reg = value;

		s_Flags[FlagNegative] = value & 0b1000'0000;
		s_Flags[FlagZero] = value == 0;

		return OpValue{ 3, 4 };
	}

//	static auto LdAbsoluteReg(CPU& cpu, std::uint8_t Registers::* reg, std::uint8_t Registers::* offsetReg, std::string_view regString) -> std::optional<OpValue>
//	{
//		auto addressLow = cpu.ReadAddress(s_Registers.PC + 1);
//		auto addressHigh = cpu.ReadAddress(s_Registers.PC + 2);
//		std::uint16_t address = (addressHigh << 8) + addressLow + s_Registers.*offsetReg;
//		PrintCommandArgAbsolute(address, regString);
//		address += s_Registers.*offsetReg;
//
//		s_Registers.*reg = cpu.ReadAddress(address);
//
//		s_Flags[FlagNegative] = s_Registers.*reg & 0b1000'0000;
//		s_Flags[FlagZero] = s_Registers.*reg == 0;
//
//		bool boundaryCrossed = (((s_Registers.PC + s_Registers.*reg) & 0xFF00) != (s_Registers.PC & 0xFF00));
//
//		return OpValue{ 3, static_cast<std::uint8_t>(4 + boundaryCrossed ? 1 : 0) };
//	}

	static auto LdAbsoluteReg(CPU& cpu, std::uint8_t Registers::* reg, std::uint8_t Registers::* offsetReg, std::string_view regString) -> std::optional<OpValue>
	{
		auto value = cpu.ReadAbsoluteAddressRegister(offsetReg);

		s_Registers.*reg = value;

		s_Flags[FlagNegative] = value & 0b1000'0000;
		s_Flags[FlagZero] = value == 0;

		bool boundaryCrossed = (((s_Registers.PC + s_Registers.*offsetReg) & 0xFF00) != (s_Registers.PC & 0xFF00));

		return OpValue{ 3, static_cast<std::uint8_t>(4 + boundaryCrossed ? 1 : 0) };
	}

//	static auto LdImmediate(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
//	{
//		auto value = cpu.ReadAddress(s_Registers.PC + 1);
//		PrintCommandArg(value, true);
//
//		s_Registers.*reg = cpu.ReadAddress(value);
//
//		s_Flags[FlagNegative] = s_Registers.*reg & 0b1000'0000;
//		s_Flags[FlagZero] = s_Registers.*reg == 0;
//
//		return OpValue{ 2, 2 };
//	}

	// TODO: Here is a thing that differs, but this solution seems to be the correct one...
	static auto LdImmediate(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		auto value = cpu.ReadAddress(s_Registers.PC + 1);

		s_Registers.*reg = value;

		s_Flags[FlagNegative] = value & 0b1000'0000;
		s_Flags[FlagZero] = value == 0;

		return OpValue{ 2, 2 };
	}

//	static auto StAbsolute(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
//	{
//		auto addressLow = cpu.ReadAddress(s_Registers.PC + 1);
//		auto addressHigh = cpu.ReadAddress(s_Registers.PC + 2);
//		std::uint16_t address = (addressHigh << 8) + addressLow;
//		PrintCommandArg(address);
//
//		cpu.WriteAddress(address, s_Registers.*reg);
//
//		return OpValue{ 3, 4 };
//	}

	static auto StAbsolute(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		cpu.WriteAbsoluteAddress(s_Registers.*reg);

		return OpValue{ 3, 4 };
	}

//	static auto StZeropage(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
//	{
//		auto zeropageAddress = cpu.ReadAddress(s_Registers.PC + 1);
//		PrintCommandArg(zeropageAddress);
//
//		cpu.WriteAddress(zeropageAddress, s_Registers.*reg);
//
//		return OpValue{ 2, 3 };
//	}

	static auto StZeropage(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		cpu.WriteZeropageAddress(s_Registers.*reg);

		return OpValue{ 2, 3 };
	}

//	static auto StaAbsoluteReg(CPU& cpu, std::uint8_t Registers::* reg, std::string_view regStr) -> std::optional<OpValue>
//	{
//		auto addressLow = cpu.ReadAddress(s_Registers.PC + 1);
//		auto addressHigh = cpu.ReadAddress(s_Registers.PC + 2);
//		std::uint16_t address = (addressHigh << 8) + addressLow;
//
//		PrintCommandArgIndirect(address, regStr);
//		address += s_Registers.*reg;
//
//		cpu.WriteAddress(address, s_Registers.A);
//
//		return OpValue{ 3, 5 };
//	}

	static auto StaAbsoluteReg(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		cpu.WriteAbsoluteAddressRegister(reg, s_Registers.A);

		return OpValue{ 3, 5 };
	}

	static auto StaIndirect(CPU& cpu, std::uint8_t Registers::* reg, std::string_view regStr) -> std::optional<OpValue>
	{
		auto zeropageAddress = cpu.ReadAddress(s_Registers.PC + 1);

		auto addressLow = cpu.ReadAddress(zeropageAddress);
		auto addressHigh = cpu.ReadAddress(zeropageAddress + 1);
		std::uint16_t address = (addressHigh << 8) + addressLow;
		PrintCommandArgIndirect(address, regStr);
		address += s_Registers.*reg;

		cpu.WriteAddress(address, s_Registers.A);

		return OpValue{ 2, 6 };
	}

	static auto StaIndirectIndexed(CPU& cpu) -> std::optional<OpValue>
	{
		cpu.WriteIndirectIndexed(s_Registers.A);

		return OpValue{ 2, 6 };
	}

//	static auto TransferA(std::uint8_t Registers::* reg) -> std::optional<OpValue>
//	{
//		s_Registers.*reg = s_Registers.A;
//
//		s_Flags[FlagNegative] = s_Registers.*reg & 0b1000'0000;
//		s_Flags[FlagZero] = s_Registers.*reg == 0;
//
//		return OpValue{ 1, 2 };
//	}

	static auto Transfer(std::uint8_t Registers::* from, std::uint8_t Registers::* to) -> std::optional<OpValue>
	{
		s_Registers.*to = s_Registers.*from;

		s_Flags[FlagNegative] = s_Registers.*to & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.*to == 0;

		return OpValue{ 1, 2 };
	}

	static auto ExecuteOpcode(CPU& cpu, std::uint8_t opCode) -> const std::optional<OpValue>
	{
		switch (opCode)
		{
			// BPL (BPL #rel) - branch if N=0 -
			case 0x10:
			{
				return Branch(cpu, FlagNegative, false);
			}
	
	
			// JMP (JMP $xxxx) -
			case 0x20:
			{
				return JmpAbsolute(cpu);
			}

			// BIT zeropage (BIT $xx) - NVZ
			case 0x24:
			{
				return BitZeropage(cpu);
			}

			// BIT absolute (BIT #$xx) - NVZ
			case 0x2C:
			{
				return BitAbsolute(cpu);
			}

			// RTS
			case 0x60:
			{
//				std::println("RTS!");
				return ReturnFromSubroutine(cpu);
			}

			// SEI - 1-I
			case 0x78:
			{
				s_Flags[FlagInterrupt] = true;
				return OpValue{ 1, 2 };
			}

			// STA zeropage (STA $xx) - A->M  -
			case 0x85:
			{
				return StZeropage(cpu, &Registers::A);
			}

			// STX zeropage (STX $xx) - X->M -
			case 0x86:
			{
				return StZeropage(cpu, &Registers::X);
			}

			// DEY - Y-1->Y NZ
			case 0x88:
			{
				return Dec(&Registers::Y);
			}

			// STA absolute (STA $xxxx) - A->M -
			case 0x8D:
			{
				return StAbsolute(cpu, &Registers::A);
			}

			// STA indirect (STA ($xx),Y) - A->M -
			case 0x91:
			{
				return StaIndirectIndexed(cpu);
			}

			// STA abs, Y (STA #$xx,Y) -
			case 0x99:
			{
				return StaAbsoluteReg(cpu, &Registers::Y);
			}
			
			// TXS - X->SP -
			case 0x9A:
			{
				s_Registers.SP = s_Registers.X;
				return OpValue{ 1, 2 };
			}

			// LDY immediate (LDY #$xx) - M->Y NZ
			case 0xA0:
			{
				return LdImmediate(cpu, &Registers::Y);
			}

			// LDX immediate (LDX #xxxx) - M->X NZ
			case 0xA2:
			{
				return LdImmediate(cpu, &Registers::X);
			}

			// LDA immediate (LDA #$xx) - M->A  NZ
			case 0xA9:
			{
				return LdImmediate(cpu, &Registers::A);
			}

				
			// TAX - A->X NZ
			case 0xAA:
			{
				return Transfer(&Registers::A, &Registers::X);
			}

			// LDY absolute (LDY $xxxx) - M->Y NZ
			case 0xAC:
			{
				return LdAbsolute(cpu, &Registers::Y);
			}

			// LDA absolute (LDA $xx) - M->A NZ
			case 0xAD:
			{
				return LdAbsolute(cpu, &Registers::A);
			}

			// LDX absolute (LDX $xxxx) - M->X NZ
			case 0xAE:
			{
				return LdAbsolute(cpu, &Registers::X);
			}

			// BCS (BCS #rel) - branch if C=1 -
			case 0xB0:
			{
				return Branch(cpu, FlagCarry, true);
			}

			// LDA absolute X (LDA $xx,X) - M->A NZ
			case 0xBD:
			{
				return LdAbsoluteReg(cpu, &Registers::A, &Registers::X, "X");
			}

			// CPY immediate (CPY #$xx) - A-M  NZC
			case 0xC0:
			{
				return CmpImmediate(cpu, &Registers::Y);
			}

			// INY - Y+1->Y NZ
			case 0xC8:
			{
				return Inc(&Registers::Y);
			}

			// CMP immediate (CMP #$xx) - A-M  NZC
			case 0xC9:
			{
				return CmpImmediate(cpu, &Registers::A);
			}

			// DEX implied - X-1->X NZ
			case 0xCA:
			{
				return Dec(&Registers::X);
			}

			// BNE relative (BNE #xx)
			case 0xD0:
			{
				return Branch(cpu, FlagZero, false);
			}

			// CLD - 0->D
			case 0xD8:
			{
				PrintSingleCommand("CLD");
				s_Flags[FlagDecimal] = false;
				return OpValue{ 1, 2 };
			}
	
			// CPX immediate (CPX #$xx) - X-M  NZC
			case 0xE0:
			{
				PrintCommand("CPX");
				return CmpImmediate(cpu, &Registers::X);
			}
	
			default:
				std::println("Invalid opcode: {:2x}", opCode);
				return std::nullopt;

		}

		return std::nullopt;
	}


	auto CPU::Execute(std::uint16_t startVector) -> void
	{
		s_Flags[FlagInterrupt] = true;

		if (startVector == 0)
		{
//			std::uint16_t resetVector = (m_Memory.Read(s_Registers.PC + 1) << 8) + m_Memory.Read(s_Registers.PC);
//			std::println("Reset vector: {:04x}", resetVector);
			auto resetVector = 0xFFFC;
			s_Registers.PC = m_Memory.Read(resetVector + 1) << 8 + m_Memory.Read(resetVector);
		}
		else
			s_Registers.PC = startVector;

		// Sets VBLANK flag - keep it static for debugging until PPU is implemented :-)
		m_Memory.Write(0x2002, 0x80);

		const auto frequency = 1'662'607;
		const auto cycleTime = 1'000'000'000 / frequency;

		LARGE_INTEGER li{};
		if (!QueryPerformanceFrequency(&li))
		{
			std::println("QueryPerformanceFrequency failed");
		}

		auto freq = li.QuadPart; //  / 1'000.0;

//		std::println("Queried frequency: {}", freq);

//		if (freq < frequency)
//			std::println("Too high frequency for QueryPerformanceFrequency");
//
		auto frequencyDivider = static_cast<double>(freq) / frequency;

//		std::println("Frequency divider: {}", frequencyDivider);

		std::uint64_t cycles{ 0 };

		while (true)
		{
			LARGE_INTEGER startCount{};
			QueryPerformanceCounter(&startCount);

			auto maybeExecuted = ExecuteOpcode(*this, m_Memory.Read(s_Registers.PC));

			if (!maybeExecuted)
				break;

			s_Registers.PC += maybeExecuted->Size;

			cycles += maybeExecuted->ClockCycles;

			std::int64_t countsElapsed{};

			// Cycle sync
			do
			{
				LARGE_INTEGER endCount;
				QueryPerformanceCounter(&endCount);

				countsElapsed = endCount.QuadPart - startCount.QuadPart;
			} while (static_cast<double>(countsElapsed) < (maybeExecuted->ClockCycles * frequencyDivider));
		}

		std::println("Cycles: {}", cycles);
		PrintRegisters();
	}

	auto CPU::GetRegisters() -> Registers&
	{
		return s_Registers;
	}

}
