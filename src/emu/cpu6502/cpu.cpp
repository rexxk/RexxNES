#include "emu/cpu6502/cpu.h"

#include <bitset>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <vector>


namespace emu
{

#define PRINT_COMMANDS	0


	struct Registers
	{
		std::uint8_t A{};
		std::uint8_t X{};
		std::uint8_t Y{};
		std::uint16_t PC{0xFFFC};
		std::uint8_t SP{0xFD};
	};

	
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
	static std::vector<std::uint8_t> s_Memory(65536);

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

	static auto BranchCarrySet() -> std::optional<std::int16_t>
	{
		std::int8_t value = s_Memory.at(s_Registers.PC + 1);
		PrintCommandArg(value, false, true);

		if (s_Flags[FlagCarry] == true)
		{
			s_Registers.PC += value;
		}

		return 2;
	}

	static auto BranchNotZero() -> std::optional<std::int16_t>
	{
		std::int8_t value = s_Memory.at(s_Registers.PC + 1);
		PrintCommandArg(value, false, true);

		if (s_Flags[FlagZero] == false)
		{
			s_Registers.PC += value;
		}

		return 2;
	}

	static auto BranchPlus() -> std::optional<std::int16_t>
	{
		std::int8_t value = s_Memory.at(s_Registers.PC + 1);			
		PrintCommandArg(value, false, true);

		if (s_Flags[FlagNegative] == false)
		{
			s_Registers.PC += value;
		}

		return 2;
	}

	static auto CmpImmediate(std::uint8_t Registers::* reg) -> std::optional<std::uint16_t>
	{
		auto value = s_Memory.at(s_Registers.PC + 1);
		PrintCommandArg(value, true);

		auto result = s_Registers.*reg - value;
		s_Flags[FlagNegative] = result & 0b1000'0000;
		s_Flags[FlagZero] = result == 0;
		s_Flags[FlagCarry] = static_cast<int8_t>(result) >= 0;

		return 2;
	}

	static auto DeImplied(std::uint8_t Registers::* reg) -> std::optional<std::uint16_t>
	{
		s_Registers.*reg = s_Registers.*reg - 1;

		s_Flags[FlagNegative] = s_Registers.*reg & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.*reg == 0;

		return 1;
	}

	static auto InImplied(std::uint8_t Registers::* reg) -> std::optional<std::uint16_t>
	{
		s_Registers.*reg = s_Registers.*reg + 1;

		s_Flags[FlagNegative] = s_Registers.*reg & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.*reg == 0;

		return 1;
	}

	static auto JmpAbsolute() -> std::optional<std::uint16_t>
	{
		// Save PC+2 to stack
		s_Memory[StackLocation + s_Registers.SP--] = static_cast<std::uint8_t>((s_Registers.PC + 3) & 0xFF);
		s_Memory[StackLocation + s_Registers.SP--] = static_cast<std::uint8_t>(((s_Registers.PC + 3) & 0xFF00) >> 8);

		std::println("JMP from 0x{:04x}", s_Registers.PC + 3);

		auto addressLow = s_Memory.at(s_Registers.PC + 1);
		auto addressHigh = s_Memory.at(s_Registers.PC + 2);
		std::uint16_t address = (addressHigh << 8) + addressLow;
		PrintCommandArg(address);

		s_Registers.PC = address;

		return 0;
	}

	static auto ReturnFromSubroutine() -> std::optional<std::uint16_t>
	{
		auto addressHigh = s_Memory[StackLocation + ++s_Registers.SP];
		auto addressLow = s_Memory[StackLocation + ++s_Registers.SP];
		std::uint16_t address = (addressHigh << 8) + addressLow;

		std::println("RTS to 0x{:04x}", address);

		s_Registers.PC = address;

		return 0;
	}

	static auto LdAbsolute(std::uint8_t Registers::* reg) -> std::optional<std::uint16_t>
	{
		auto addressLow = s_Memory.at(s_Registers.PC + 1);
		auto addressHigh = s_Memory.at(s_Registers.PC + 2);
		std::uint16_t address = (addressHigh << 8) + addressLow;
		PrintCommandArg(address);

		s_Registers.*reg = s_Memory[address];

		s_Flags[FlagNegative] = s_Registers.*reg & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.*reg == 0;

		return 3;
	}

	static auto LdAbsoluteReg(std::uint8_t Registers::* reg, std::uint8_t Registers::* offsetReg, std::string_view regString) -> std::optional<std::uint16_t>
	{
		auto addressLow = s_Memory.at(s_Registers.PC + 1);
		auto addressHigh = s_Memory.at(s_Registers.PC + 2);
		std::uint16_t address = (addressHigh << 8) + addressLow + s_Registers.*offsetReg;
		PrintCommandArgAbsolute(address, regString);
		address += s_Registers.*offsetReg;

		s_Registers.*reg = s_Memory[address];

		s_Flags[FlagNegative] = s_Registers.*reg & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.*reg == 0;

		return 3;
	}

	static auto LdImmediate(std::uint8_t Registers::* reg) -> std::optional<std::uint16_t>
	{
		auto value = s_Memory.at(s_Registers.PC + 1);
		PrintCommandArg(value, true);

		s_Registers.*reg = s_Memory[value];

		s_Flags[FlagNegative] = s_Registers.*reg & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.*reg == 0;

		return 2;
	}

	static auto StAbsolute(std::uint8_t Registers::* reg) -> std::optional<std::uint16_t>
	{
		auto addressLow = s_Memory.at(s_Registers.PC + 1);
		auto addressHigh = s_Memory.at(s_Registers.PC + 2);
		std::uint16_t address = (addressHigh << 8) + addressLow;
		PrintCommandArg(address);

		s_Memory.at(address) = s_Registers.*reg;

		return 3;
	}

	static auto StZeropage(std::uint8_t Registers::* reg) -> std::optional<std::uint16_t>
	{
		auto zeropageAddress = s_Memory.at(s_Registers.PC + 1);
		PrintCommandArg(zeropageAddress);

		s_Memory.at(zeropageAddress) = s_Registers.*reg;

		return 2;
	}

	static auto StaIndirect(std::uint8_t Registers::* reg, std::string_view regStr) -> std::optional<std::uint16_t>
	{
		auto zeropageAddress = s_Memory.at(s_Registers.PC + 1);

		auto addressLow = s_Memory.at(zeropageAddress);
		auto addressHigh = s_Memory.at(zeropageAddress + 1);
		std::uint16_t address = (addressHigh << 8) + addressLow;
		PrintCommandArgIndirect(address, regStr);
		address += s_Registers.*reg;

		s_Memory.at(address) = s_Registers.A;

		return 2;
	}

	static auto TransferA(std::uint8_t Registers::* reg) -> std::optional<std::uint16_t>
	{
		s_Registers.*reg = s_Registers.A;

		s_Flags[FlagNegative] = s_Registers.*reg & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.*reg == 0;

		return 1;
	}

	static auto ExecuteOpcode() -> const std::optional<std::uint16_t>
	{
		std::optional<std::uint16_t> dataSize{ 0 };

		auto opCode = s_Memory.at(s_Registers.PC);

		switch (opCode)
		{
			// BPL (BPL #rel) - branch if N=0 -
			case 0x10:
			{
				PrintCommand("BPL");
				return BranchPlus();
			}

			// JMP (JMP $xxxx) -
			case 0x20:
			{
				PrintCommand("JMP");
				return JmpAbsolute();
			}

			case 0x60:
			{
				PrintSingleCommand("RTS");
				return ReturnFromSubroutine();
			}

			// SEI - 1-I
			case 0x78:
			{
				PrintSingleCommand("SEI");
				s_Flags[FlagInterrupt] = true;
				return 1;
			}

			// STA zeropage (STA $xx) - A->M  -
			case 0x85:
			{
				PrintCommand("STA");
				return StZeropage(&Registers::A);
			}

			// STX zeropage (STX $xx) - X->M -
			case 0x86:
			{
				PrintCommand("STX");
				return StZeropage(&Registers::X);
			}

			// DEY - Y-1->Y NZ
			case 0x88:
			{
				PrintSingleCommand("DEY");
				return DeImplied(&Registers::Y);
			}

			// STA absolute (STA $xxxx) - A->M -
			case 0x8D:
			{
				PrintCommand("STA");
				return StAbsolute(&Registers::A);
			}

			// STA indirect (STA ($xx),Y) - A->M -
			case 0x91:
			{
				PrintCommand("STA");
				return StaIndirect(&Registers::Y, "Y");
			}

			// TXS - X->SP -
			case 0x9A:
			{
				PrintSingleCommand("TXS");
				s_Registers.SP = s_Registers.X;
				return 1;
			}

			// LDX immediate (LDX #xxxx) - M->X NZ
			case 0xA2:
			{
				PrintCommand("LDX");
				return LdImmediate(&Registers::X);
			}

			// LDA immediate (LDA #$xx) - M->A  NZ
			case 0xA9:
			{
				PrintCommand("LDA");
				return LdImmediate(&Registers::A);
			}
	
			// LDY immediate (LDY #$xx) - M->Y NZ
			case 0xA0:
			{
				PrintCommand("LDY");
				return LdImmediate(&Registers::Y);
			}

			// TAX - A->X NZ
			case 0xAA:
			{
				PrintSingleCommand("TAX");
				return TransferA(&Registers::X);
			}

			// LDA absolute (LDA $xx) - M->A NZ
			case 0xAD:
			{
				PrintCommand("LDA");
				return LdAbsolute(&Registers::A);
			}

			// BCS (BCS #rel) - branch if C=1 -
			case 0xB0:
			{
				PrintCommand("BCS");
				return BranchCarrySet();
			}

			// LDA absolute X (LDA $xx,X) - M->A NZ
			case 0xBD:
			{
				PrintCommand("LDA");
				return LdAbsoluteReg(&Registers::A, &Registers::X, "X");
			}

			// CPY immediate (CPY #$xx) - A-M  NZC
			case 0xC0:
			{
				PrintCommand("CPY");
				return CmpImmediate(&Registers::Y);
			}

			// INY - Y+1->Y NZ
			case 0xC8:
			{
				PrintSingleCommand("INY");
				return InImplied(&Registers::Y);
			}

			// CMP immediate (CMP #$xx) - A-M  NZC
			case 0xC9:
			{
				PrintCommand("CMP");
				return CmpImmediate(&Registers::A);
			}

			// DEX implied - X-1->X NZ
			case 0xCA:
			{
				PrintSingleCommand("DEX");
				return DeImplied(&Registers::X);
			}

			// BNE relative (BNE #xx)
			case 0xD0:
			{
				PrintCommand("BNE");
				return BranchNotZero();
			}

			// CLD - 0->D
			case 0xD8:
			{
				PrintSingleCommand("CLD");
				s_Flags[FlagDecimal] = false;
				return 1;
			}

			// CPX immediate (CPX #$xx) - X-M  NZC
			case 0xE0:
			{
				PrintCommand("CPX");
				return CmpImmediate(&Registers::X);
			}

			default:
				std::println("Invalid opcode: {:2x}", opCode);
				return std::nullopt;
	
			}
	
		return dataSize;
	}


	auto CPU::Execute() -> void
	{
		s_Flags[FlagInterrupt] = true;

		std::uint16_t resetVector = (s_Memory.at(s_Registers.PC + 1) << 8) + s_Memory.at(s_Registers.PC);

		std::println("Reset vector: {:04x}", resetVector);

		s_Registers.PC = resetVector;

		// Sets VBLANK flag - keep it static for debugging until PPU is implemented :-)
		SetMemory(0x2002, 0x80);

		while (s_Memory.at(s_Registers.PC) != 0u)
		{
			auto maybeExecuted = ExecuteOpcode();

			if (maybeExecuted.has_value())
				s_Registers.PC += maybeExecuted.value();
			else
				break;
		}

		PrintRegisters();
	}


#if 0
	auto CPU::Execute(std::span<uint8_t> program, const std::uint16_t memoryLocation) -> void
	{
		if ((memoryLocation + program.size()) > s_Memory.size())
		{
			std::println("Program memory location is out of bounds");
			return;
		}

		// Place program in memory
		std::copy(std::begin(program), std::end(program), s_Memory.data() + memoryLocation);

		// Setup program counter
//		s_Registers.PC = memoryLocation;
		s_Flags[FlagInterrupt] = true;

		while (s_Memory.at(s_Registers.PC) != 0u)
		{
//			std::println("@{:x}: {:x}", s_Registers.PC, s_Memory.at(s_Registers.PC));

			auto maybeExecuted = ExecuteOpcode();

			if (maybeExecuted.has_value())
				s_Registers.PC += maybeExecuted.value();
			else
				break;
		}

		PrintRegisters();
//		for (auto& op : program)
//		{
//			std::println("Data: {:x}", op);
//
//
//
//		}

	}
#endif

	auto CPU::InstallROM(std::uint16_t address, std::span<uint8_t> romData) -> void
	{
		if (address + romData.size() > s_Memory.size())
		{
			std::println("CPU::InstallROM - Not enough space for ROM in memory at location {}", address);
			return;
		}

		std::copy(std::begin(romData), std::end(romData), s_Memory.data() + address);
	}


	auto CPU::SetMemory(std::uint16_t location, std::uint8_t value) -> void
	{
		if (location > s_Memory.size())
		{
			std::println("CPU::SetMemory - Invalid location");
			return;
		}

		s_Memory.at(location) = value;
	}


}
