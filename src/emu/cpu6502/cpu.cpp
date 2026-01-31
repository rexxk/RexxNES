#include "emu/cpu6502/cpu.h"

#include <atomic>
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

	static std::atomic<bool> s_NMI{ false };
	static std::atomic<bool> s_IRQ{ false };

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

	static auto Break(CPU& cpu) -> std::optional<OpValue>
	{
//		cpu.WriteAddress(StackLocation + s_Registers.SP--, static_cast<std::uint8_t>(((s_Registers.PC + 2) & 0xFF00) >> 8));
//		cpu.WriteAddress(StackLocation + s_Registers.SP--, static_cast<std::uint8_t>((s_Registers.PC + 2) & 0xFF));

		cpu.WriteAddress(StackLocation + s_Registers.SP--, static_cast<std::uint8_t>(((s_Registers.PC) & 0xFF00) >> 8));
		cpu.WriteAddress(StackLocation + s_Registers.SP--, static_cast<std::uint8_t>((s_Registers.PC) & 0xFF));

		s_Flags[FlagInterrupt] = true;

		cpu.WriteAddress(StackLocation + s_Registers.SP--, static_cast<std::uint8_t>(s_Flags.to_ulong()));

		return OpValue{ 1, 7 };
	}

	static auto Branch(CPU& cpu, const std::uint8_t flag, bool condition) -> std::optional<OpValue>
	{
		std::int8_t relativePosition = cpu.ReadAddress(s_Registers.PC + 1);
		PrintCommandArg(relativePosition, false, true);

		s_Registers.PC += 2;

		bool boundaryCrossed = ((s_Registers.PC + relativePosition) & 0xFF00) != (s_Registers.PC & 0xFF00);

		if (s_Flags[flag] == condition)
		{
			s_Registers.PC += relativePosition;
		}

		return OpValue{ 0, static_cast<std::uint8_t>(2 + (boundaryCrossed ? 2 : 1)) };
	}

	static auto Bit(std::uint8_t value) -> void
	{
		PrintCommandArg(value);

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
		auto addressLow = cpu.ReadAddress(s_Registers.PC + 1);
		auto addressHigh = cpu.ReadAddress(s_Registers.PC + 2);
		std::uint16_t address = (addressHigh << 8) + addressLow;

		PrintCommandArg(address);

		s_Registers.PC = address;

		return OpValue{ 0, 3 };
	}

	static auto JmpIndirect(CPU& cpu) -> std::optional<OpValue>
	{
		auto addressLow = cpu.ReadAddress(s_Registers.PC + 1);
		auto addressHigh = cpu.ReadAddress(s_Registers.PC + 2);
		std::uint16_t address = (addressHigh << 8) + addressLow;

#if PRINT_COMMANDS
		std::println("   $({:04x})", address);
#endif

		auto jumpAddressLow = cpu.ReadAddress(address);
		auto jumpAddressHigh = cpu.ReadAddress(address + 1);
		std::uint16_t jumpAddress = (jumpAddressHigh << 8) + jumpAddressLow;

		s_Registers.PC = jumpAddress;

		// TODO: fix cycles
		return OpValue{ 0, 5 };
	}

	static auto JsrAbsolute(CPU& cpu) -> std::optional<OpValue>
	{
		cpu.WriteAddress(StackLocation + s_Registers.SP--, static_cast<std::uint8_t>(((s_Registers.PC + 3) & 0xFF00) >> 8));
		cpu.WriteAddress(StackLocation + s_Registers.SP--, static_cast<std::uint8_t>((s_Registers.PC + 3) & 0xFF));

		//		std::println("JMP from 0x{:04x}", s_Registers.PC + 3);

		auto addressLow = cpu.ReadAddress(s_Registers.PC + 1);
		auto addressHigh = cpu.ReadAddress(s_Registers.PC + 2);
		std::uint16_t address = (addressHigh << 8) + addressLow;

		PrintCommandArg(address);

		s_Registers.PC = address;

		return OpValue{ 0, 3 };
	}

	static auto ReturnFromSubroutine(CPU& cpu) -> std::optional<OpValue>
	{
		auto addressLow = cpu.ReadAddress(StackLocation + ++s_Registers.SP);
		auto addressHigh = cpu.ReadAddress(StackLocation + ++s_Registers.SP);
		std::uint16_t address = (addressHigh << 8) + addressLow;

//		std::println("RTS to 0x{:04x}", address);
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


	auto CPU::FetchAbsluteAddressRegister(std::uint8_t Registers::* reg) -> std::uint16_t
	{
		auto memoryLow = m_Memory.Read(s_Registers.PC + 1);
		auto memoryHigh = m_Memory.Read(s_Registers.PC + 2);
		std::uint16_t address = (memoryHigh << 8) + memoryLow + s_Registers.*reg;

		return address;
	}

	auto CPU::FetchAddress() -> std::uint16_t
	{
		auto memoryLow = m_Memory.Read(s_Registers.PC + 1);
		auto memoryHigh = m_Memory.Read(s_Registers.PC + 2);
		std::uint16_t address = (memoryHigh << 8) + memoryLow;

		return address;
	}

	auto CPU::FetchZeropageAddress() -> std::uint16_t
	{
		auto memoryLow = m_Memory.Read(s_Registers.PC + 1);
		std::uint16_t address = (0x00 << 8) + memoryLow;

		return address;
	}

	auto CPU::ReadAbsoluteAddress() -> std::uint8_t
	{
		auto memoryLow = m_Memory.Read(s_Registers.PC + 1);
		auto memoryHigh = m_Memory.Read(s_Registers.PC + 2);
		std::uint16_t address = (memoryHigh << 8) + memoryLow;

		PrintCommandArg(address);

		return m_Memory.Read(address);
	}

	auto CPU::ReadAbsoluteAddressRegister(std::uint8_t Registers::* reg, std::string_view regString) -> std::uint8_t
	{
		auto memoryLow = m_Memory.Read(s_Registers.PC + 1);
		auto memoryHigh = m_Memory.Read(s_Registers.PC + 2);
		std::uint16_t address = (memoryHigh << 8) + memoryLow + s_Registers.*reg;

		PrintCommandArgAbsolute(address, regString);

		return m_Memory.Read(address);
	}

	auto CPU::ReadIndirectIndexed() -> std::uint8_t
	{
		auto zeropageAddress = m_Memory.Read(s_Registers.PC + 1);

		auto addressLow = m_Memory.Read(zeropageAddress);
		auto addressHigh = m_Memory.Read(zeropageAddress + 1);
		std::uint16_t address = (addressHigh << 8) + addressLow;

		address += s_Registers.Y;

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

		PrintCommandArg(address);

		m_Memory.Write(address, value);
	}

	auto CPU::WriteAbsoluteAddressRegister(std::uint8_t Registers::* reg, const std::uint8_t value, std::string_view regString) -> void
	{
		auto memoryLow = m_Memory.Read(s_Registers.PC + 1);
		auto memoryHigh = m_Memory.Read(s_Registers.PC + 2);
		std::uint16_t address = (memoryHigh << 8) + memoryLow + s_Registers.*reg;

		PrintCommandArgAbsolute(address, regString);

		m_Memory.Write(address, value);
	}

	auto CPU::WriteIndirectIndexed(std::uint8_t value) -> void
	{
		auto zeropageAddress = m_Memory.Read(s_Registers.PC + 1);
		PrintCommandArgIndirect(zeropageAddress, "Y");

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

		PrintCommandArg(address);

		m_Memory.Write(address, value);
	}


	static auto AndAbsoluteOffset(CPU& cpu, std::uint8_t Registers::* reg, std::string_view regString) -> std::optional<OpValue>
	{
		auto value = cpu.ReadAbsoluteAddressRegister(reg, regString);

		PrintCommandArgAbsolute(value, regString);

		s_Registers.A = s_Registers.A & value;

		// TODO: Add boundary check

		s_Flags[FlagNegative] = s_Registers.A & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.A == 0;

		return OpValue{ 3, 4 };
	}


	static auto AndImmediate(CPU& cpu) -> std::optional<OpValue>
	{
		auto value = cpu.ReadAddress(s_Registers.PC + 1);

		PrintCommandArg(value);

		s_Registers.A = s_Registers.A & value;

		s_Flags[FlagNegative] = s_Registers.A & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.A == 0;

		return OpValue{ 2, 2 };
	}

	static auto AslAccumulator(CPU& cpu) -> std::optional<OpValue>
	{
		s_Flags[FlagCarry] = s_Registers.A & 0x80;
		s_Registers.A <<= 1;

		return OpValue{ 1, 2 };
	}


	static auto CmpAbsolute(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		auto value = cpu.ReadAbsoluteAddress();

//		PrintCommandArg(value);

		std::uint8_t result = s_Registers.*reg - value;
		s_Flags[FlagNegative] = result & 0b1000'0000;
		s_Flags[FlagZero] = result == 0;
		s_Flags[FlagCarry] = result >= 0;

		return OpValue{ 3, 4 };
	}

	static auto CmpImmediate(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		auto value = cpu.ReadAddress(s_Registers.PC + 1);
		PrintCommandArg(value, true);

//		if (s_Registers.X == 1)
//			__debugbreak();

		std::uint8_t result = s_Registers.*reg - value;

//		s_Flags[FlagNegative] = static_cast<std::int8_t>(result) & 0b1000'0000;
		s_Flags[FlagNegative] = result & 0b1000'0000;
		s_Flags[FlagZero] = result == 0;
		s_Flags[FlagCarry] = result >= 0;


		return OpValue{ 2, 2 };
	}

	static auto Dec(std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		s_Registers.*reg = s_Registers.*reg - 1;

		s_Flags[FlagNegative] = s_Registers.*reg & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.*reg == 0;

		return OpValue{ 1, 2 };
	}

	static auto DecAbsolute(CPU& cpu) -> std::optional<OpValue>
	{
		auto address = cpu.FetchAddress();
		auto value = cpu.ReadAddress(address);

		value--;

		cpu.WriteAddress(address, value);

		s_Flags[FlagNegative] = value & 0b1000'0000;
		s_Flags[FlagZero] = value == 0;

		return OpValue{ 3, 6 };
	}

	static auto EorZeropage(CPU& cpu) -> std::optional<OpValue>
	{
		auto address = cpu.FetchZeropageAddress();
		auto value = cpu.ReadZeropageAddress();

		PrintCommandArg(address);

		s_Registers.A = s_Registers.A ^ value;

		s_Flags[FlagNegative] = s_Registers.A & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.A == 0;

		return OpValue{ 2, 3 };
	}

	static auto Inc(std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		s_Registers.*reg = s_Registers.*reg + 1;

		s_Flags[FlagNegative] = s_Registers.*reg & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.*reg == 0;

		return OpValue{ 1, 2 };
	}

	static auto IncAbsolute(CPU& cpu) -> std::optional<OpValue>
	{
		auto addressLow = cpu.ReadAddress(s_Registers.PC + 1);
		auto addressHigh = cpu.ReadAddress(s_Registers.PC + 2);
		std::uint16_t address = (addressHigh << 8) + addressLow;

		auto value = cpu.ReadAddress(address);
		value++;
		cpu.WriteAddress(address, value);

		s_Flags[FlagNegative] = value & 0b1000'0000;
		s_Flags[FlagZero] = value == 0;

		return OpValue{ 3, 6 };
	}

	static auto IncZeropage(CPU& cpu) -> std::optional<OpValue>
	{
		auto address = cpu.FetchZeropageAddress();
		auto value = cpu.ReadZeropageAddress();
		value++;

		cpu.WriteAddress(address, value);

		s_Flags[FlagNegative] = value & 0b1000'0000;
		s_Flags[FlagZero] = value == 0;

		return OpValue{ 2, 5 };
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
		auto value = cpu.ReadAbsoluteAddressRegister(offsetReg, regString);

		s_Registers.*reg = value;

		s_Flags[FlagNegative] = value & 0b1000'0000;
		s_Flags[FlagZero] = value == 0;

		bool boundaryCrossed = (((s_Registers.PC + s_Registers.*offsetReg) & 0xFF00) != (s_Registers.PC & 0xFF00));

		return OpValue{ 3, static_cast<std::uint8_t>(4 + boundaryCrossed ? 1 : 0) };
	}

	static auto LdImmediate(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		auto value = cpu.ReadAddress(s_Registers.PC + 1);

		PrintCommandArg(value);

		s_Registers.*reg = value;

		s_Flags[FlagNegative] = value & 0b1000'0000;
		s_Flags[FlagZero] = value == 0;

		return OpValue{ 2, 2 };
	}

	static auto LdaIndirectIndex(CPU& cpu) -> std::optional<OpValue>
	{
		auto value = cpu.ReadIndirectIndexed();

		bool boundaryCrossed = (((s_Registers.PC + s_Registers.A) & 0xFF00) != (s_Registers.PC & 0xFF00));

		s_Registers.A = value;

		s_Flags[FlagNegative] = value & 0b1000'0000;
		s_Flags[FlagZero] = value == 0;

		return OpValue{ 2, static_cast<std::uint8_t>(5 + (boundaryCrossed == true) ? 1 : 0) };
	}

	static auto LdZeropage(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		auto address = cpu.FetchZeropageAddress();
		auto value = cpu.ReadZeropageAddress();

		PrintCommandArg(address);

		s_Registers.*reg = value;

		s_Flags[FlagNegative] = value & 0b1000'0000;
		s_Flags[FlagZero] = value == 0;

		return OpValue{ 2, 3 };
	}

	static auto LogicalShiftRight(CPU& cpu) -> std::optional<OpValue>
	{
		s_Flags[FlagCarry] = s_Registers.A & 0x01;

		s_Registers.A = s_Registers.A >> 1;

		s_Flags[FlagNegative] = false;
		s_Flags[FlagZero] = s_Registers.A == 0;

		return OpValue{ 1, 2 };
	}

	static auto OrAbsoluteRegister(CPU& cpu, std::uint8_t Registers::* reg, std::string_view regString) -> std::optional<OpValue>
	{
		auto value = cpu.ReadAbsoluteAddressRegister(reg, regString);
	
		s_Registers.A = s_Registers.A | value;

		s_Flags[FlagNegative] = s_Registers.A & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.A == 0;

		// TODO: Page boundary check and cycle correction
		return OpValue{ 3, 4 };
	}

	static auto OrImmediate(CPU& cpu) -> std::optional<OpValue>
	{
		auto value = cpu.ReadAddress(s_Registers.PC + 1);

		PrintCommandArg(value);

		s_Registers.A = s_Registers.A | value;

		s_Flags[FlagNegative] = s_Registers.A & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.A == 0;

		return OpValue{ 2, 2 };
	}

	static auto OrIndirectIndexed(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		auto value = cpu.ReadIndirectIndexed();

		PrintCommandArgIndirect(value, "Y");

		s_Registers.A = s_Registers.A | value;

		s_Flags[FlagNegative] = s_Registers.A & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.A == 0;

		return OpValue{ 2, 6 };
	}

	static auto OrZeropage(CPU& cpu) -> std::optional<OpValue>
	{
		auto value = cpu.ReadZeropageAddress();

		PrintCommandArg(value);

		s_Registers.A = s_Registers.A | value;

		s_Flags[FlagNegative] = s_Registers.A & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.A == 0;

		return OpValue{ 2, 3 };
	}

	static auto PullFromStack(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		s_Registers.*reg = cpu.ReadAddress(StackLocation + ++s_Registers.SP);

		return OpValue{ 1, 4 };
	}

	static auto PullSRFromStack(CPU& cpu) -> std::optional<OpValue>
	{
		s_Flags = cpu.ReadAddress(StackLocation + ++s_Registers.SP);

		return OpValue{ 1, 4 };
	}

	static auto PushSRToStack(CPU& cpu) -> std::optional<OpValue>
	{
		cpu.WriteAddress(StackLocation + s_Registers.SP--, static_cast<std::uint8_t>(s_Flags.to_ulong()));

		return OpValue{ 1, 3 };
	}

	static auto PushToStack(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		cpu.WriteAddress(StackLocation + s_Registers.SP--, s_Registers.*reg);

		return OpValue{ 1, 3 };
	}

	static auto ReturnFromInterrupt(CPU& cpu) -> std::optional<OpValue>
	{
		PullSRFromStack(cpu);
		auto addressLow = cpu.ReadAddress(StackLocation + ++s_Registers.SP);
		auto addressHigh = cpu.ReadAddress(StackLocation + ++s_Registers.SP);
		std::uint16_t address = (addressHigh << 8) + addressLow;

		std::println(" RTI ${:04x}", address);

		s_Registers.PC = address;

		return OpValue{ 0, 6 };
	}

	static auto RotateLeft(CPU& cpu) -> std::optional<OpValue>
	{
		std::uint16_t value = s_Registers.A << 1;
		bool carryFlag = (value & 0x100);
		value += carryFlag;

		s_Flags[FlagCarry] = carryFlag;
		s_Flags[FlagNegative] = static_cast<std::uint8_t>(value) & 0b1000'0000;
		s_Flags[FlagZero] = static_cast<std::uint8_t>(value) == 0;

		s_Registers.A = static_cast<std::uint8_t>(value);

		return OpValue{ 1, 2 };
	}

	static auto RotateLeftAbsoluteX(CPU& cpu) -> std::optional<OpValue>
	{
		auto address = cpu.FetchAbsluteAddressRegister(&Registers::X);
		PrintCommandArgAbsolute(address, "X");

		std::uint16_t value = (std::uint8_t(0x00 << 8) + cpu.ReadAddress(address)) << 1;
		bool carryFlag = (value & 0x100);
		value += carryFlag;

		s_Flags[FlagCarry] = carryFlag;
		s_Flags[FlagNegative] = static_cast<std::uint8_t>(value) & 0b1000'0000;
		s_Flags[FlagZero] = static_cast<std::uint8_t>(value) == 0;

		cpu.WriteAddress(address, static_cast<std::uint8_t>(value));

		return OpValue{ 3, 7 };
	}

	static auto RotateRightAbsoluteX(CPU& cpu) -> std::optional<OpValue>
	{
		auto address = cpu.FetchAbsluteAddressRegister(&Registers::X);
		PrintCommandArgAbsolute(address, "X");

		auto value = cpu.ReadAddress(address);
		bool carry = s_Flags[FlagCarry];
		bool newCarryFlag = value & 0x01;

		value = (value >> 1) + (static_cast<std::uint8_t>(carry) & 0x01 << 7);

		s_Flags[FlagCarry] = newCarryFlag;
		s_Flags[FlagNegative] = static_cast<std::uint8_t>(value) & 0b1000'0000;
		s_Flags[FlagZero] = static_cast<std::uint8_t>(value) == 0;

		cpu.WriteAddress(address, static_cast<std::uint8_t>(value));

		return OpValue{ 3, 7 };
	}

	static auto SbcAbsoluteOffset(CPU& cpu, std::uint8_t Registers::* reg, std::string_view regString) -> std::optional<OpValue>
	{
		bool boundaryCrossed = (((s_Registers.PC + s_Registers.*reg) & 0xFF00) != (s_Registers.PC & 0xFF00));

		auto value = cpu.ReadAbsoluteAddressRegister(reg, regString);
		
		auto result = s_Registers.A + (0xFF - value) + s_Flags[FlagCarry];

		s_Flags[FlagZero] = result == 0;
		s_Flags[FlagNegative] = result & 0x80;
		s_Flags[FlagCarry] = result >= 0;
		s_Flags[FlagOverflow] = ((s_Registers.A & 0x80) && !(value & 0x80)) || (!(s_Registers.A & 0x80) && (value & 0x80));

		return OpValue{ 3, static_cast<std::uint8_t>(4 + boundaryCrossed ? 1 : 0) };
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
		cpu.WriteAbsoluteAddressRegister(reg, s_Registers.A, "A");

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
			// BRK
			case 0x00:
			{
				PrintSingleCommand("BRK");
				return Break(cpu);
			}
	
			// ORA zeropage (ORA $xx) - A OR M -> A - NZ
			case 0x05:
			{
				PrintCommand("ORA");
				return OrZeropage(cpu);
			}

			// PHP
			case 0x08:
			{
				PrintSingleCommand("PHP");
				return PushSRToStack(cpu);
			}

			// ORA (ORA #$xx) - NZ
			case 0x09:
			{
				PrintCommand("ORA");
				return OrImmediate(cpu);
			}

			// ASL accumulator (ASL) - NZC
			case 0x0a:
			{
				PrintSingleCommand("ASL");
				return AslAccumulator(cpu);
			}

			// BPL (BPL #rel) - branch if N=0 -
			case 0x10:
			{
				PrintCommand("BPL");
				return Branch(cpu, FlagNegative, false);
			}

			// ORA (ORA ($xx),Y) -  NZ
			case 0x11:
			{
				PrintCommand("ORA");
				return OrIndirectIndexed(cpu, &Registers::A);
			}
	
			// CLC
			case 0x18:
			{
				PrintSingleCommand("CLC");
				s_Flags[FlagCarry] = false;
				return OpValue{ 1, 2 };
			}

			// ORA absolute,Y (ORA $xxxx,Y) - A OR M -> A - NZ
			case 0x19:
			{
				PrintCommand("ORA");
				return OrAbsoluteRegister(cpu, &Registers::Y, "Y");
			}

			// JSR (JSR $xxxx) -
			case 0x20:
			{
				PrintCommand("JSR");
				return JsrAbsolute(cpu);
			}

			// BIT zeropage (BIT $xx) - NVZ
			case 0x24:
			{
				PrintCommand("BIT");
				return BitZeropage(cpu);
			}

			// PLP
			case 0x28:
			{
				PrintSingleCommand("PLP");
				return PullSRFromStack(cpu);
			}

			// AND immediate (AND #$xx) - NZ
			case 0x29:
			{
				PrintCommand("AND");
				return AndImmediate(cpu);
			}

			// ROL accumulator (ROL) - NZC
			case 0x2A:
			{
				PrintSingleCommand("ROL");
				return RotateLeft(cpu);
			}

			// BIT absolute (BIT #$xx) - NVZ
			case 0x2C:
			{
				PrintCommand("BIT");
				return BitAbsolute(cpu);
			}

			// SEC implied (SEC)
			case 0x38:
			{
				PrintSingleCommand("SEC");
				s_Flags[FlagCarry] = true;
				return OpValue{ 1, 2 };
			}

			// AND absolute, X (AND $xxxx,X) - A AND M -> A - NZ
			case 0x3D:
			{
				PrintCommand("AND");
				return AndAbsoluteOffset(cpu, &Registers::X, "X");
			}

			// ROL absolute,X (ROL $xxxx,X) - NZC
			case 0x3E:
			{
				PrintCommand("ROL");
				return RotateLeftAbsoluteX(cpu);
			}

			// RTI
			case 0x40:
			{
				PrintSingleCommand("RTI");
				return ReturnFromInterrupt(cpu);
			}

			// EOR zeropage (EOR $xx) - A EOR M -> A - NZ
			case 0x45:
			{
				PrintCommand("EOR");
				return EorZeropage(cpu);
			}

			// PHA implied (PHA)
			case 0x48:
			{
				PrintSingleCommand("PHA");
				return PushToStack(cpu, &Registers::A);
			}

			// LSR A (LSR) - (N0)ZC
			case 0x4A:
			{
				PrintSingleCommand("LSR");
				return LogicalShiftRight(cpu);
			}

			// JMP absolute (JMP $xxxx)
			case 0x4C:
			{
				PrintCommand("JMP");
				return JmpAbsolute(cpu);
			}

			// RTS
			case 0x60:
			{
				PrintSingleCommand("RTS");
//				std::println("RTS!");
				return ReturnFromSubroutine(cpu);
			}

			// PLA
			case 0x68:
			{
				PrintSingleCommand("PLA");
				return PullFromStack(cpu, &Registers::A);
			}

			// JMP indirect (JMP $(xxxx))
			case 0x6C:
			{
				PrintCommand("JMP");
				return JmpIndirect(cpu);
			}

			// SEI - 1-I
			case 0x78:
			{
				PrintSingleCommand("SEI");
				s_Flags[FlagInterrupt] = true;
				return OpValue{ 1, 2 };
			}

			// ROR absolute,X (ROR $xxxx,X) - C-> x -> C - NZC
			case 0x7E:
			{
				PrintCommand("ROR");
				return RotateRightAbsoluteX(cpu);
			}

			// STA zeropage (STA $xx) - A->M  -
			case 0x85:
			{
				PrintCommand("STA");
				return StZeropage(cpu, &Registers::A);
			}

			// STX zeropage (STX $xx) - X->M -
			case 0x86:
			{
				PrintCommand("STX");
				return StZeropage(cpu, &Registers::X);
			}

			// DEY - Y-1->Y NZ
			case 0x88:
			{
				PrintSingleCommand("DEY");
				return Dec(&Registers::Y);
			}

			// TXA
			case 0x8A:
			{
				PrintSingleCommand("TXA");
				return Transfer(&Registers::X, &Registers::A);
			}
			// STA absolute (STA $xxxx) - A->M -
			case 0x8D:
			{
				PrintCommand("STA");
				return StAbsolute(cpu, &Registers::A);
			}

			// BCC
			case 0x90:
			{
				PrintCommand("BCC");
				return Branch(cpu, FlagCarry, false);
			}

			// STA indirect (STA ($xx),Y) - A->M -
			case 0x91:
			{
				PrintCommand("STA");
				return StaIndirectIndexed(cpu);
			}

			// STA abs, Y (STA #$xx,Y) -
			case 0x99:
			{
				PrintCommand("STA");
				return StaAbsoluteReg(cpu, &Registers::Y);
			}
			
			// TXS - X->SP -
			case 0x9A:
			{
				PrintSingleCommand("TXS");
				s_Registers.SP = s_Registers.X;
				return OpValue{ 1, 2 };
			}

			// STA absolute X (STA $xx, X) - A->M
			case 0x9D:
			{
				PrintCommand("STA");
				return StaAbsoluteReg(cpu, &Registers::X);
			}

			// LDY immediate (LDY #$xx) - M->Y NZ
			case 0xA0:
			{
				PrintCommand("LDY imm");
				return LdImmediate(cpu, &Registers::Y);
			}

			// LDX immediate (LDX #xxxx) - M->X NZ
			case 0xA2:
			{
				PrintCommand("LDX");
				return LdImmediate(cpu, &Registers::X);
			}

			// LDA zeropage (LDA $xx) - M->A - NZ
			case 0xA5:
			{
				PrintCommand("LDA");
				return LdZeropage(cpu, &Registers::A);
			}

			// TAY
			case 0xA8:
			{
				PrintSingleCommand("TAY");
				return Transfer(&Registers::A, &Registers::Y);
			}

			// LDA immediate (LDA #$xx) - M->A  NZ
			case 0xA9:
			{
				PrintCommand("LDA");
				return LdImmediate(cpu, &Registers::A);
			}

				
			// TAX - A->X NZ
			case 0xAA:
			{
				PrintCommand("TAX");
				return Transfer(&Registers::A, &Registers::X);
			}

			// LDY absolute (LDY $xxxx) - M->Y NZ
			case 0xAC:
			{
				PrintCommand("LDY");
				return LdAbsolute(cpu, &Registers::Y);
			}

			// LDA absolute (LDA $xx) - M->A NZ
			case 0xAD:
			{
				PrintCommand("LDA");
				return LdAbsolute(cpu, &Registers::A);
			}

			// LDX absolute (LDX $xxxx) - M->X NZ
			case 0xAE:
			{
				PrintCommand("LDX");
				return LdAbsolute(cpu, &Registers::X);
			}

			// BCS (BCS #rel) - branch if C=1 -
			case 0xB0:
			{
				PrintCommand("BCS");
				return Branch(cpu, FlagCarry, true);
			}

			// LDA indirect, Y (LDA $xx),Y - NZ
			case 0xB1:
			{
				PrintCommand("LDA");
				return LdaIndirectIndex(cpu);
			}

			// LDA absolute X (LDA $xx,X) - M->A NZ
			case 0xBD:
			{
				PrintCommand("LDA");
				return LdAbsoluteReg(cpu, &Registers::A, &Registers::X, "X");
			}

			// LDX absolute Y (LDX $xx,Y) - M->Z NZ
			case 0xBE:
			{
				PrintCommand("LDX");
				return LdAbsoluteReg(cpu, &Registers::X, &Registers::Y, "Y");
			}

			// CPY immediate (CPY #$xx) - A-M  NZC
			case 0xC0:
			{
				PrintCommand("CPY");
				return CmpImmediate(cpu, &Registers::Y);
			}

			// INY - Y+1->Y NZ
			case 0xC8:
			{
				PrintSingleCommand("INY");
				return Inc(&Registers::Y);
			}

			// CMP immediate (CMP #$xx) - A-M  NZC
			case 0xC9:
			{
				PrintCommand("CMP");
				return CmpImmediate(cpu, &Registers::A);
			}

			// DEX implied - X-1->X NZ
			case 0xCA:
			{
				PrintSingleCommand("DEX");
				return Dec(&Registers::X);
			}

			// CPY absolute (CMP $xxxx) - Y-M  NZC
			case 0xCC:
			{
				PrintCommand("CPY");
				return CmpAbsolute(cpu, &Registers::Y);
			}

			// DEC absolute (DEC $xxxx) - M-1->M  NZ
			case 0xCE:
			{
				PrintCommand("DEC");
				return DecAbsolute(cpu);
			}

			// BNE relative (BNE #xx)
			case 0xD0:
			{
				PrintCommand("BNE");
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

			// INC zeropage (INC $xx)
			case 0xE6:
			{
				PrintCommand("INC");
				return IncZeropage(cpu);
			}

			// INX implied
			case 0xE8:
			{
				PrintSingleCommand("INX");
				return Inc(&Registers::X);
			}

			// INC absolute (INC $xxxx)
			case 0xEE:
			{
				PrintCommand("INC");
				return IncAbsolute(cpu);
			}

			// BEQ relative (BEQ #xx)
			case 0xF0:
			{
				PrintCommand("BEQ");
				return Branch(cpu, FlagZero, true);
			}

			// SBC absolute, Y (SBC $xxxx,Y) - A - M - C' -> A  - NZCV
			case 0xF9:
			{
				PrintCommand("SBC");
				return SbcAbsoluteOffset(cpu, &Registers::Y, "Y");
			}

			default:
				std::println("Invalid opcode: {:02x} @{:04x}", opCode, s_Registers.PC);
				return std::nullopt;

		}

		return std::nullopt;
	}


	auto CPU::Execute(std::uint16_t startVector) -> void
	{
		m_Executing.store(true);

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

		while (m_Executing.load())
		{
			LARGE_INTEGER startCount{};
			QueryPerformanceCounter(&startCount);

			if (s_NMI.load()) // && !s_Flags[FlagInterrupt])
			{
				s_NMI.store(false);
				std::println("NMI called - PC: ${:04x}", s_Registers.PC);

				Break(*this);
				// PC = 0xFFFA - 1
				s_Registers.PC = 0xFFF9;

				JmpAbsolute(*this);
			}
			else if (s_IRQ.load()) // && !s_Flags[FlagInterrupt])
			{
				s_IRQ.store(false);
//				std::println("IRQ called - PC: ${:04x}", s_Registers.PC);%
				std::println("IRQ vector is unused in NES");

//				Break(*this);
				// PC = 0xFFFE - 1
//				s_Registers.PC = 0xFFFD;

//				JmpAbsolute(*this);
			}


			auto maybeExecuted = ExecuteOpcode(*this, m_Memory.Read(s_Registers.PC));

			if (!maybeExecuted)
				break;

			s_Registers.PC += maybeExecuted->Size;

			cycles += maybeExecuted->ClockCycles;

			if (cycles >= 3'000'000 && cycles < 3'000'005)
			{
				s_NMI.store(true);
//				s_IRQ.store(true);
//				break;
			}

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

	auto CPU::GetFlags() -> const std::uint8_t
	{
		return static_cast<std::uint8_t>(s_Flags.to_ulong());
	}

}
