#include "emu/cpu6502/cpu.h"
#include "input/controller.h"

#include <atomic>
#include <array>
#include <bitset>
#include <chrono>
#include <functional>
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
	static std::atomic<bool> s_NMIRunning{ false };
	static std::atomic<bool> s_IRQ{ false };
	static std::atomic<bool> s_StepToRTS{ false };

	static std::uint8_t s_DMACycles{ 0 };

	struct OpValue
	{
		std::uint8_t Size{ 0 };
		std::uint8_t ClockCycles{ 0 };
	};

	using OpCodeFn = std::function<std::optional<OpValue>(CPU&)>;
	static std::array<OpCodeFn, 0xFF> s_OpCodes;

	enum class FrequencyType
	{
		PAL,
		NTSC,
		Dendy,
	};

	static std::unordered_map<FrequencyType, std::uint32_t> s_Frequency
	{
		{ FrequencyType::PAL, 1'662'607 },
		{ FrequencyType::NTSC, 1'789'773 },
		{ FrequencyType::Dendy, 1'773'448 },
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





	auto CPU::FetchAbsoluteAddress() -> std::uint16_t
	{
		auto memoryLow = ReadAddress(s_Registers.PC + 1);
		auto memoryHigh = ReadAddress(s_Registers.PC + 2);
		std::uint16_t address = (memoryHigh << 8) + memoryLow;

		return address;
	}

	auto CPU::FetchAbsluteAddressRegister(std::uint8_t Registers::* reg) -> std::uint16_t
	{
		auto memoryLow = ReadAddress(s_Registers.PC + 1);
		auto memoryHigh = ReadAddress(s_Registers.PC + 2);
		std::uint16_t address = (memoryHigh << 8) + memoryLow + s_Registers.*reg;

		return address;
	}

	auto CPU::FetchIndirectIndexedAddress() -> std::uint16_t
	{
		auto zeropageAddress = m_MemoryManager.ReadMemory(MemoryOwner::CPU, s_Registers.PC + 1);

		auto addressLow = ReadAddress(zeropageAddress);
		auto addressHigh = ReadAddress(zeropageAddress + 1);
		std::uint16_t address = (addressHigh << 8) + addressLow;

		address += s_Registers.Y;

		return address;
	}

	auto CPU::FetchZeropageAddress() -> std::uint16_t
	{
		auto memoryLow = ReadAddress(s_Registers.PC + 1);
		std::uint16_t address = (0x00 << 8) + memoryLow;

		return address;
	}

	auto CPU::FetchZeropageAddressRegister(std::uint8_t Registers::*offset) -> std::uint16_t
	{
		auto memoryLow = ReadAddress(s_Registers.PC + 1);
		std::uint16_t address = (0x00 << 8) + memoryLow + s_Registers.*offset;

		return address;
	}

	auto CPU::ReadAddress(std::uint16_t address) -> std::uint8_t
	{
		return m_MemoryManager.ReadMemory(MemoryOwner::CPU, address);
	}

	auto CPU::ReadAbsoluteAddress() -> std::uint8_t
	{
		return ReadAddress(FetchAbsoluteAddress());
	}

	auto CPU::ReadAbsoluteAddressRegister(std::uint8_t Registers::* reg) -> std::uint8_t
	{
		return ReadAddress(FetchAbsluteAddressRegister(reg));
	}

	auto CPU::ReadIndirectIndexed() -> std::uint8_t
	{
		return ReadAddress(FetchIndirectIndexedAddress());
	}

	auto CPU::ReadZeropageAddress() -> std::uint8_t
	{
		return ReadAddress(FetchZeropageAddress());
	}

	auto CPU::ReadZeropageAddressRegister(std::uint8_t Registers::*reg) -> std::uint8_t
	{
		return ReadAddress(FetchZeropageAddressRegister(reg));
	}

	auto CPU::WriteAddress(std::uint16_t address, std::uint8_t value) -> void
	{
		// Check for OAM DMA write
		if (address == 0x4014)
		{
			m_MemoryManager.DMATransfer(MemoryOwner::PPU, value);
			s_DMACycles = 514;
		}

		if (address == 0x4015)
		{
			m_MemoryManager.DMATransfer(MemoryOwner::ASU, value);
			s_DMACycles = 4;
		}

		return m_MemoryManager.WriteMemory(MemoryOwner::CPU, address, value);
	}

	auto CPU::WriteAbsoluteAddress(const std::uint8_t value) -> void
	{
		WriteAddress(FetchAbsoluteAddress(), value);
	}

	auto CPU::WriteAbsoluteAddressRegister(std::uint8_t Registers::* reg, const std::uint8_t value) -> void
	{
		WriteAddress(FetchAbsluteAddressRegister(reg), value);
	}

	auto CPU::WriteIndirectIndexed(std::uint8_t value) -> void
	{
		WriteAddress(FetchIndirectIndexedAddress(), value);
	}

	auto CPU::WriteZeropageAddress(const std::uint8_t value) -> void
	{
		WriteAddress(FetchZeropageAddress(),value);
	}

	auto CPU::WriteZeropageAddressRegister(std::uint8_t Registers::* offset, const std::uint8_t value) -> void
	{
		WriteAddress(FetchZeropageAddressRegister(offset), value);
	}






	auto AddWithCarry(std::uint8_t value) -> void
	{
		bool bit7 = value & 0x80;
		std::uint16_t result = s_Registers.A + value + s_Flags[FlagCarry];
		s_Registers.A = static_cast<std::uint8_t>(result);

		s_Flags[FlagCarry] = result & 0x100;
		s_Flags[FlagNegative] = s_Registers.A & 0b1000'0000;
		s_Flags[FlagOverflow] = (s_Registers.A & 0b0100'0000) ^ bit7;
		s_Flags[FlagZero] = s_Registers.A == 0;
	}

	static auto AddWithCarryAbsolute(CPU& cpu) -> std::optional<OpValue>
	{
		AddWithCarry(cpu.ReadAbsoluteAddress());

		return OpValue{ 3, 4 };
	}
	
	static auto AddWithCarryAbsoluteIndexed(CPU& cpu, std::uint8_t Registers::*reg) -> std::optional<OpValue>
	{
		AddWithCarry(cpu.ReadAbsoluteAddressRegister(reg));

		return OpValue{ 3, 4 };
	}

	static auto AddWithCarryImmediate(CPU& cpu) -> std::optional<OpValue>
	{
		AddWithCarry(cpu.ReadAddress(s_Registers.PC + 1));

		return OpValue{ 2, 2 };
	}

	static auto AddWithCarryZeropage(CPU& cpu) -> std::optional<OpValue>
	{
		AddWithCarry(cpu.ReadZeropageAddress());

		return OpValue{ 2, 3 };
	}

	static auto AddWithCarryZeropageReg(CPU& cpu, std::uint8_t Registers::*reg) -> std::optional<OpValue>
	{
		AddWithCarry(cpu.ReadZeropageAddressRegister(reg));

		return OpValue{ 2, 4 };
	}

	auto And(std::uint8_t value) -> void
	{
		s_Registers.A = s_Registers.A & value;

		s_Flags[FlagNegative] = s_Registers.A & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.A == 0;
	}

	static auto AndAbsolute(CPU& cpu) -> std::optional<OpValue>
	{
		And(cpu.ReadAbsoluteAddress());

		return OpValue{ 3, 4 };
	}

	static auto AndAbsoluteOffset(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		And(cpu.ReadAbsoluteAddressRegister(reg));

		return OpValue{ 3, 4 };
	}


	static auto AndImmediate(CPU& cpu) -> std::optional<OpValue>
	{
		And(cpu.ReadAddress(s_Registers.PC + 1));

		return OpValue{ 2, 2 };
	}

	static auto AndZeropage(CPU& cpu) -> std::optional<OpValue>
	{
		And(cpu.ReadZeropageAddress());

		return OpValue{ 2, 3 };
	}

	auto ArithmeticShiftLeft(std::uint8_t& value) -> void
	{
		s_Flags[FlagCarry] = value & 0x80;
		value <<= 1;

		s_Flags[FlagNegative] = value & 0b1000'0000;
		s_Flags[FlagZero] = value == 0;
	}

	static auto AslAccumulator(CPU& cpu) -> std::optional<OpValue>
	{
		ArithmeticShiftLeft(s_Registers.A);

		return OpValue{ 1, 2 };
	}

	static auto AslAbsolute(CPU& cpu) -> std::optional<OpValue>
	{
		auto address = cpu.FetchAbsoluteAddress();
		auto value = cpu.ReadAbsoluteAddress();

		ArithmeticShiftLeft(value);

		cpu.WriteAddress(address, value);

		return OpValue{ 3, 6 };
	}

	auto Bit(std::uint8_t value) -> void
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

	static auto Branch(CPU& cpu, const std::uint8_t flag, bool condition) -> std::optional<OpValue>
	{
		std::int8_t relativePosition = cpu.ReadAddress(s_Registers.PC + 1);

		s_Registers.PC += 2;

		bool boundaryCrossed = ((s_Registers.PC + relativePosition) & 0xFF00) != (s_Registers.PC & 0xFF00);

		if (s_Flags[flag] == condition)
		{
			s_Registers.PC += relativePosition;
		}

		return OpValue{ 0, static_cast<std::uint8_t>(2 + (boundaryCrossed ? 2 : 1)) };
	}

	static auto Break(CPU& cpu) -> std::optional<OpValue>
	{
		cpu.WriteAddress(StackLocation + s_Registers.SP--, static_cast<std::uint8_t>(((s_Registers.PC) & 0xFF00) >> 8));
		cpu.WriteAddress(StackLocation + s_Registers.SP--, static_cast<std::uint8_t>((s_Registers.PC) & 0xFF));

		s_Flags[FlagInterrupt] = true;

		cpu.WriteAddress(StackLocation + s_Registers.SP--, static_cast<std::uint8_t>(s_Flags.to_ulong()));

		s_Flags[FlagBreak] = true;

		return OpValue{ 1, 7 };
	}

	auto Compare(std::uint8_t Registers::*reg, std::uint8_t value) -> void
	{
		std::uint8_t result = s_Registers.*reg - value;
		s_Flags[FlagNegative] = result & 0b1000'0000;
		s_Flags[FlagZero] = result == 0;
		s_Flags[FlagCarry] = result >= 0;
	}

	static auto CmpAbsolute(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		Compare(reg, cpu.ReadAbsoluteAddress());

		return OpValue{ 3, 4 };
	}

	static auto CmpAbsoluteIndexed(CPU& cpu, std::uint8_t Registers::* reg, std::uint8_t Registers::* offset) -> std::optional<OpValue>
	{
		Compare(reg, cpu.ReadAbsoluteAddressRegister(offset));

		return OpValue{ 3, 4 };
	}

	static auto CmpImmediate(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		Compare(reg, cpu.ReadAddress(s_Registers.PC + 1));

		return OpValue{ 2, 2 };
	}

	static auto CmpZeropage(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		Compare(reg, cpu.ReadZeropageAddress());

		return OpValue{ 2, 3 };
	}

	static auto CmpZeropageReg(CPU& cpu, std::uint8_t Registers::* reg, std::uint8_t Registers::* offset) -> std::optional<OpValue>
	{
		Compare(reg, cpu.ReadZeropageAddressRegister(offset));

		return OpValue{ 2, 4 };
	}

	auto DecreaseRegister(std::uint8_t Registers::* reg) -> void
	{
		s_Registers.*reg = s_Registers.*reg - 1;

		s_Flags[FlagNegative] = s_Registers.*reg & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.*reg == 0;
	}

	auto DecreaseValue(std::uint8_t value) -> std::uint8_t
	{
		value = value - 1;

		s_Flags[FlagNegative] = value & 0b1000'0000;
		s_Flags[FlagZero] = value == 0;

		return value;
	}

	static auto Dec(std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		DecreaseRegister(reg);

		return OpValue{ 1, 2 };
	}

	static auto DecAbsolute(CPU& cpu) -> std::optional<OpValue>
	{
		auto address = cpu.FetchAbsoluteAddress();
		auto value = cpu.ReadAddress(address);

		value = DecreaseValue(value);

		cpu.WriteAddress(address, value);

		return OpValue{ 3, 6 };
	}

	static auto DecAbsoluteRegister(CPU& cpu, std::uint8_t Registers::*reg) -> std::optional<OpValue>
	{
		auto address = cpu.FetchAbsluteAddressRegister(reg);
		auto value = cpu.ReadAbsoluteAddressRegister(reg);

		value = DecreaseValue(value);

		cpu.WriteAddress(address, value);

		return OpValue{ 3, 7 };
	}

	static auto DecZeropage(CPU& cpu) -> std::optional<OpValue>
	{
		auto address = cpu.FetchZeropageAddress();
		auto value = cpu.ReadZeropageAddress();

		value = DecreaseValue(value);

		cpu.WriteAddress(address, value);

		return OpValue{ 2, 5 };
	}

	static auto DecZeropageReg(CPU& cpu, std::uint8_t Registers::*reg) -> std::optional<OpValue>
	{
		auto address = cpu.FetchZeropageAddressRegister(reg);
		auto value = cpu.ReadZeropageAddressRegister(reg);

		value = DecreaseValue(value);

		cpu.WriteAddress(address, value);

		return OpValue{ 2, 6 };
	}

	auto ExclusiveOr(std::uint8_t value) -> void
	{
		s_Registers.A = s_Registers.A ^ value;

		s_Flags[FlagNegative] = s_Registers.A & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.A == 0;
	}

	static auto EorImmediate(CPU& cpu) -> std::optional<OpValue>
	{
		ExclusiveOr(cpu.ReadAddress(s_Registers.PC + 1));

		return OpValue{ 2, 2 };
	}

	static auto EorZeropage(CPU& cpu) -> std::optional<OpValue>
	{
		ExclusiveOr(cpu.ReadZeropageAddress());

		return OpValue{ 2, 3 };
	}

	auto IncreaseRegister(std::uint8_t Registers::* reg) -> void
	{
		s_Registers.*reg = s_Registers.*reg + 1;

		s_Flags[FlagNegative] = s_Registers.*reg & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.*reg == 0;
	}

	auto IncreaseValue(std::uint8_t value) -> std::uint8_t
	{
		value = value + 1;

		s_Flags[FlagNegative] = value & 0b1000'0000;
		s_Flags[FlagZero] = value == 0;

		return value;
	}

	static auto Inc(std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		IncreaseRegister(reg);

		return OpValue{ 1, 2 };
	}

	static auto IncAbsolute(CPU& cpu) -> std::optional<OpValue>
	{
		std::uint16_t address = cpu.FetchAbsoluteAddress();
		auto value = cpu.ReadAddress(address);

		value = IncreaseValue(value);

		cpu.WriteAddress(address, value);

		return OpValue{ 3, 6 };
	}

	static auto IncZeropage(CPU& cpu) -> std::optional<OpValue>
	{
		auto address = cpu.FetchZeropageAddress();
		auto value = cpu.ReadZeropageAddress();

		value = IncreaseValue(value);

		cpu.WriteAddress(address, value);

		return OpValue{ 2, 5 };
	}

	static auto JmpAbsolute(CPU& cpu) -> std::optional<OpValue>
	{
		auto addressLow = cpu.ReadAddress(s_Registers.PC + 1);
		auto addressHigh = cpu.ReadAddress(s_Registers.PC + 2);
		std::uint16_t address = (addressHigh << 8) + addressLow;

		s_Registers.PC = address;

		return OpValue{ 0, 3 };
	}

	static auto JmpIndirect(CPU& cpu) -> std::optional<OpValue>
	{
		auto addressZeroPage = cpu.ReadAddress(s_Registers.PC + 1);

		auto jumpAddressLow = cpu.ReadAddress(addressZeroPage);
		auto jumpAddressHigh = cpu.ReadAddress(addressZeroPage + 1);
		std::uint16_t jumpAddress = (jumpAddressHigh << 8) + jumpAddressLow;

		s_Registers.PC = jumpAddress;

		// TODO: fix cycles
		return OpValue{ 0, 5 };
	}

	static auto JsrAbsolute(CPU& cpu) -> std::optional<OpValue>
	{
		cpu.WriteAddress(StackLocation + s_Registers.SP--, static_cast<std::uint8_t>(((s_Registers.PC + 2) & 0xFF00) >> 8));
		cpu.WriteAddress(StackLocation + s_Registers.SP--, static_cast<std::uint8_t>((s_Registers.PC + 2) & 0xFF));

		auto addressLow = cpu.ReadAddress(s_Registers.PC + 1);
		auto addressHigh = cpu.ReadAddress(s_Registers.PC + 2);
		std::uint16_t address = (addressHigh << 8) + addressLow;

		s_Registers.PC = address;

		return OpValue{ 0, 3 };
	}

	auto LoadRegister(std::uint8_t Registers::* reg, std::uint8_t value) -> void
	{
		s_Registers.*reg = value;

		s_Flags[FlagNegative] = value & 0b1000'0000;
		s_Flags[FlagZero] = value == 0;
	}

	static auto LdAbsolute(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		LoadRegister(reg, cpu.ReadAbsoluteAddress());

		return OpValue{ 3, 4 };
	}

	static auto LdAbsoluteReg(CPU& cpu, std::uint8_t Registers::* reg, std::uint8_t Registers::* offsetReg) -> std::optional<OpValue>
	{
		LoadRegister(reg, cpu.ReadAbsoluteAddressRegister(offsetReg));

		bool boundaryCrossed = (((s_Registers.PC + s_Registers.*offsetReg) & 0xFF00) != (s_Registers.PC & 0xFF00));

		return OpValue{ 3, static_cast<std::uint8_t>(4 + boundaryCrossed ? 1 : 0) };
	}

	static auto LdImmediate(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		LoadRegister(reg, cpu.ReadAddress(s_Registers.PC + 1));

		return OpValue{ 2, 2 };
	}

	static auto LdaIndirectIndex(CPU& cpu) -> std::optional<OpValue>
	{
		LoadRegister(&Registers::A, cpu.ReadIndirectIndexed());

		bool boundaryCrossed = (((s_Registers.PC + s_Registers.A) & 0xFF00) != (s_Registers.PC & 0xFF00));

		return OpValue{ 2, static_cast<std::uint8_t>(5 + (boundaryCrossed == true) ? 1 : 0) };
	}

	static auto LdZeropage(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		LoadRegister(reg, cpu.ReadZeropageAddress());

		return OpValue{ 2, 3 };
	}

	static auto LdZeropageReg(CPU& cpu, std::uint8_t Registers::* reg, std::uint8_t Registers::* offsetReg) -> std::optional<OpValue>
	{
		LoadRegister(reg, cpu.ReadZeropageAddressRegister(offsetReg));

		return OpValue{ 2, 4 };
	}

	auto LogicalShiftRight(std::uint8_t value) -> std::uint8_t
	{
		s_Flags[FlagCarry] = value & 0x01;

		value = value >> 1;

		s_Flags[FlagNegative] = false;
		s_Flags[FlagZero] = value == 0;

		return value;
	}

	static auto LogicalShiftRightAccumulator(CPU& cpu) -> std::optional<OpValue>
	{
		s_Registers.A = LogicalShiftRight(s_Registers.A);

		return OpValue{ 1, 2 };
	}

	static auto LogicalShiftRightAbsolute(CPU& cpu) -> std::optional<OpValue>
	{
		auto address = cpu.FetchAbsoluteAddress();
		auto value = cpu.ReadAbsoluteAddress();

		value = LogicalShiftRight(value);

		cpu.WriteAddress(address, value);

		return OpValue{ 3, 6 };
	}

	static auto LogicalShiftRightZeropage(CPU& cpu) -> std::optional<OpValue>
	{
		auto address = cpu.FetchZeropageAddress();
		auto value = cpu.ReadZeropageAddress();

		value = LogicalShiftRight(value);

		cpu.WriteAddress(address, value);

		return OpValue{ 2, 5 };
	}

	auto Or(std::uint8_t value) -> void
	{
		s_Registers.A = s_Registers.A | value;

		s_Flags[FlagNegative] = s_Registers.A & 0b1000'0000;
		s_Flags[FlagZero] = s_Registers.A == 0;
	}

	static auto OrAbsolute(CPU& cpu) -> std::optional<OpValue>
	{
		Or(cpu.ReadAbsoluteAddress());

		return OpValue{ 3, 4 };
	}

	static auto OrAbsoluteRegister(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		Or(cpu.ReadAbsoluteAddressRegister(reg));

		// TODO: Page boundary check and cycle correction
		return OpValue{ 3, 4 };
	}

	static auto OrImmediate(CPU& cpu) -> std::optional<OpValue>
	{
		Or(cpu.ReadAddress(s_Registers.PC + 1));

		return OpValue{ 2, 2 };
	}

	static auto OrIndirectIndexed(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		Or(cpu.ReadIndirectIndexed());

		return OpValue{ 2, 6 };
	}

	static auto OrZeropage(CPU& cpu) -> std::optional<OpValue>
	{
		Or(cpu.ReadZeropageAddress());

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

		s_Flags[FlagBreak] = true;

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

		s_Registers.PC = address;

		s_NMIRunning.store(false);

		return OpValue{ 0, 6 };
	}

	static auto ReturnFromSubroutine(CPU& cpu) -> std::optional<OpValue>
	{
		auto addressLow = cpu.ReadAddress(StackLocation + ++s_Registers.SP);
		auto addressHigh = cpu.ReadAddress(StackLocation + ++s_Registers.SP);
		std::uint16_t address = (addressHigh << 8) + addressLow;

		s_Registers.PC = address + 1;

		return OpValue{ 0, 6 };
	}

	auto RotateLeft(std::uint8_t value) -> std::uint8_t
	{
		bool carryFlag = (value & 0x80);
		value <<= 1;
		value += s_Flags[FlagCarry];

		s_Flags[FlagCarry] = carryFlag;
		s_Flags[FlagNegative] = static_cast<std::uint8_t>(value) & 0b1000'0000;
		s_Flags[FlagZero] = static_cast<std::uint8_t>(value) == 0;

		return value;
	}

	static auto RotateLeftAccumulator(CPU& cpu) -> std::optional<OpValue>
	{
		s_Registers.A = RotateLeft(s_Registers.A);

		return OpValue{ 1, 2 };
	}

	static auto RotateLeftZeropage(CPU& cpu) -> std::optional<OpValue>
	{
		auto address = cpu.FetchZeropageAddress();
		auto value = cpu.ReadZeropageAddress();

		value = RotateLeft(value);

		cpu.WriteAddress(address, value);

		return OpValue{ 2, 5 };
	}

	static auto RotateLeftAbsolute(CPU& cpu) -> std::optional<OpValue>
	{
		auto address = cpu.FetchAbsoluteAddress();
		std::uint8_t value = cpu.ReadAddress(address);

		value = RotateLeft(value);

		cpu.WriteAddress(address, value);

		return OpValue{ 3, 7 };
	}

	static auto RotateLeftAbsoluteX(CPU& cpu) -> std::optional<OpValue>
	{
		auto address = cpu.FetchAbsluteAddressRegister(&Registers::X);
		std::uint8_t value = cpu.ReadAddress(address);

		value = RotateLeft(value);

		cpu.WriteAddress(address, value);

		return OpValue{ 3, 7 };
	}

	auto RotateRight(std::uint8_t value) -> std::uint8_t
	{
		bool carryFlag = value & 0x01;
		value = value >> 1 + s_Flags[FlagCarry] << 7;

		s_Flags[FlagCarry] = carryFlag;
		s_Flags[FlagNegative] = value & 0b1000'0000;
		s_Flags[FlagZero] = value == 0;

		return value;
	}

	static auto RotateRightAccumulator(CPU& cpu) -> std::optional<OpValue>
	{
		s_Registers.A = RotateRight(s_Registers.A);

		return OpValue{ 1, 2 };
	}

	static auto RotateRightAbsoluteX(CPU& cpu) -> std::optional<OpValue>
	{
		auto address = cpu.FetchAbsluteAddressRegister(&Registers::X);
		auto value = cpu.ReadAddress(address);

		value = RotateRight(value);

		cpu.WriteAddress(address, value);

		return OpValue{ 3, 7 };
	}

	auto SubtractWithCarry(std::uint8_t value) -> void
	{
		s_Registers.A = s_Registers.A + (0xFF - value) + s_Flags[FlagCarry];

		s_Flags[FlagZero] = s_Registers.A == 0;
		s_Flags[FlagNegative] = s_Registers.A & 0x80;
		s_Flags[FlagCarry] = s_Registers.A >= 0;
		s_Flags[FlagOverflow] = ((s_Registers.A & 0x80) && !(value & 0x80)) || (!(s_Registers.A & 0x80) && (value & 0x80));
	}

	static auto SbcAbsolute(CPU& cpu) -> std::optional<OpValue>
	{
		SubtractWithCarry(cpu.ReadAbsoluteAddress());

		return OpValue{ 3, 4 };
	}

	static auto SbcAbsoluteOffset(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		SubtractWithCarry(cpu.ReadAbsoluteAddressRegister(reg));
		
		bool boundaryCrossed = (((s_Registers.PC + s_Registers.*reg) & 0xFF00) != (s_Registers.PC & 0xFF00));

		return OpValue{ 3, static_cast<std::uint8_t>(4 + boundaryCrossed ? 1 : 0) };
	}

	static auto SbcImmediate(CPU& cpu) -> std::optional<OpValue>
	{
		SubtractWithCarry(cpu.ReadAddress(s_Registers.PC + 1));

		return OpValue{ 2, 2 };
	}

	static auto SbcZeropage(CPU& cpu) -> std::optional<OpValue>
	{
		SubtractWithCarry(cpu.ReadZeropageAddress());

		return OpValue{ 2, 3 };
	}

	static auto SbcZeropageOffset(CPU& cpu, std::uint8_t Registers::* reg) -> std::optional<OpValue>
	{
		SubtractWithCarry(cpu.ReadZeropageAddressRegister(reg));

		return OpValue{ 2, 4 };
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

	static auto StZeropageReg(CPU& cpu, std::uint8_t Registers::* reg, std::uint8_t Registers::* offset) -> std::optional<OpValue>
	{
		cpu.WriteZeropageAddressRegister(offset, s_Registers.*reg);

		return OpValue{ 2, 4 };
	}

	static auto StaAbsoluteReg(CPU& cpu, std::uint8_t Registers::* offset) -> std::optional<OpValue>
	{
		cpu.WriteAbsoluteAddressRegister(offset, s_Registers.A);

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

	static auto NOP(CPU& cpu) -> std::optional<OpValue> { return OpValue{ 0, 0 }; }

	static auto SetupOpCodeArray() -> void
	{
		s_OpCodes[0x00] = []([[maybe_unused]] CPU& cpu) { return Break(cpu); };
		s_OpCodes[0x05] = []([[maybe_unused]] CPU& cpu) { return OrZeropage(cpu); };
		s_OpCodes[0x08] = []([[maybe_unused]] CPU& cpu) { return PushSRToStack(cpu); };
		s_OpCodes[0x08] = []([[maybe_unused]] CPU& cpu) { return PushSRToStack(cpu); };
		s_OpCodes[0x09] = []([[maybe_unused]] CPU& cpu) { return OrImmediate(cpu); };
		s_OpCodes[0x0a] = []([[maybe_unused]] CPU& cpu) { return AslAccumulator(cpu); };
		s_OpCodes[0x0d] = []([[maybe_unused]] CPU& cpu) { return OrAbsolute(cpu); };
		s_OpCodes[0x0e] = []([[maybe_unused]] CPU& cpu) { return AslAbsolute(cpu); };
		s_OpCodes[0x10] = []([[maybe_unused]] CPU& cpu) { return Branch(cpu, FlagNegative, false); };
		s_OpCodes[0x11] = []([[maybe_unused]] CPU& cpu) { return OrIndirectIndexed(cpu, &Registers::A); };
		s_OpCodes[0x18] = []([[maybe_unused]] CPU& cpu) { s_Flags[FlagCarry] = false; return OpValue{ 1, 2 }; };
		s_OpCodes[0x19] = []([[maybe_unused]] CPU& cpu) { return OrAbsoluteRegister(cpu, &Registers::Y); };
		s_OpCodes[0x20] = []([[maybe_unused]] CPU& cpu) { return JsrAbsolute(cpu); };
		s_OpCodes[0x24] = []([[maybe_unused]] CPU& cpu) { return BitZeropage(cpu); };
		s_OpCodes[0x25] = []([[maybe_unused]] CPU& cpu) { return AndZeropage(cpu); };
		s_OpCodes[0x26] = []([[maybe_unused]] CPU& cpu) { return RotateLeftZeropage(cpu); };
		s_OpCodes[0x28] = []([[maybe_unused]] CPU& cpu) { return PullSRFromStack(cpu); };
		s_OpCodes[0x29] = []([[maybe_unused]] CPU& cpu) { return AndImmediate(cpu); };
		s_OpCodes[0x2a] = []([[maybe_unused]] CPU& cpu) { return RotateLeftAccumulator(cpu); };
		s_OpCodes[0x2c] = []([[maybe_unused]] CPU& cpu) { return BitAbsolute(cpu); };
		s_OpCodes[0x2d] = []([[maybe_unused]] CPU& cpu) { return AndAbsolute(cpu); };
		s_OpCodes[0x2e] = []([[maybe_unused]] CPU& cpu) { return RotateLeftAbsolute(cpu); };
		s_OpCodes[0x30] = []([[maybe_unused]] CPU& cpu) { return Branch(cpu, FlagNegative, true); };
		s_OpCodes[0x38] = []([[maybe_unused]] CPU& cpu) { s_Flags[FlagCarry] = true; return OpValue{ 1, 2 }; };
		s_OpCodes[0x39] = []([[maybe_unused]] CPU& cpu) { return AndAbsoluteOffset(cpu, &Registers::Y); };
		s_OpCodes[0x3d] = []([[maybe_unused]] CPU& cpu) { return AndAbsoluteOffset(cpu, &Registers::X); };
		s_OpCodes[0x3e] = []([[maybe_unused]] CPU& cpu) { return RotateLeftAbsoluteX(cpu); };
		s_OpCodes[0x40] = []([[maybe_unused]] CPU& cpu) { return ReturnFromInterrupt(cpu); };
		s_OpCodes[0x45] = []([[maybe_unused]] CPU& cpu) { return EorZeropage(cpu); };
		s_OpCodes[0x46] = []([[maybe_unused]] CPU& cpu) { return LogicalShiftRightZeropage(cpu); };
		s_OpCodes[0x48] = []([[maybe_unused]] CPU& cpu) { return PushToStack(cpu, &Registers::A); };
		s_OpCodes[0x49] = []([[maybe_unused]] CPU& cpu) { return EorImmediate(cpu); };
		s_OpCodes[0x4a] = []([[maybe_unused]] CPU& cpu) { return LogicalShiftRightAccumulator(cpu); };
		s_OpCodes[0x4c] = []([[maybe_unused]] CPU& cpu) { return JmpAbsolute(cpu); };
		s_OpCodes[0x4e] = []([[maybe_unused]] CPU& cpu) { return LogicalShiftRightAbsolute(cpu); };
		s_OpCodes[0x60] = []([[maybe_unused]] CPU& cpu) { return ReturnFromSubroutine(cpu); };
		s_OpCodes[0x65] = []([[maybe_unused]] CPU& cpu) { return AddWithCarryZeropage(cpu); };
		s_OpCodes[0x68] = []([[maybe_unused]] CPU& cpu) { return PullFromStack(cpu, &Registers::A); };
		s_OpCodes[0x69] = []([[maybe_unused]] CPU& cpu) { return AddWithCarryImmediate(cpu); };
		s_OpCodes[0x6a] = []([[maybe_unused]] CPU& cpu) { return RotateRightAccumulator(cpu); };
		s_OpCodes[0x6c] = []([[maybe_unused]] CPU& cpu) { return JmpIndirect(cpu); };
		s_OpCodes[0x6d] = []([[maybe_unused]] CPU& cpu) { return AddWithCarryAbsolute(cpu); };
		s_OpCodes[0x75] = []([[maybe_unused]] CPU& cpu) { return AddWithCarryZeropageReg(cpu, &Registers::X); };
		s_OpCodes[0x78] = []([[maybe_unused]] CPU& cpu) { s_Flags[FlagInterrupt] = true;  return OpValue{ 1, 2 }; };
		s_OpCodes[0x79] = []([[maybe_unused]] CPU& cpu) { return AddWithCarryAbsoluteIndexed(cpu, &Registers::Y); };
		s_OpCodes[0x7d] = []([[maybe_unused]] CPU& cpu) { return AddWithCarryAbsoluteIndexed(cpu, &Registers::X); };
		s_OpCodes[0x7e] = []([[maybe_unused]] CPU& cpu) { return RotateRightAbsoluteX(cpu); };
		s_OpCodes[0x84] = []([[maybe_unused]] CPU& cpu) { return StZeropage(cpu, &Registers::Y); };
		s_OpCodes[0x85] = []([[maybe_unused]] CPU& cpu) { return StZeropage(cpu, &Registers::A); };
		s_OpCodes[0x86] = []([[maybe_unused]] CPU& cpu) { return StZeropage(cpu, &Registers::X); };
		s_OpCodes[0x88] = []([[maybe_unused]] CPU& cpu) { return Dec(&Registers::Y); };
		s_OpCodes[0x8a] = []([[maybe_unused]] CPU& cpu) { return Transfer(&Registers::X, &Registers::A); };
		s_OpCodes[0x8c] = []([[maybe_unused]] CPU& cpu) { return StAbsolute(cpu, &Registers::Y); };
		s_OpCodes[0x8d] = []([[maybe_unused]] CPU& cpu) { return StAbsolute(cpu, &Registers::A); };
		s_OpCodes[0x8e] = []([[maybe_unused]] CPU& cpu) { return StAbsolute(cpu, &Registers::X); };
		s_OpCodes[0x90] = []([[maybe_unused]] CPU& cpu) { return Branch(cpu, FlagCarry, false); };
		s_OpCodes[0x91] = []([[maybe_unused]] CPU& cpu) { return StaIndirectIndexed(cpu); };
		s_OpCodes[0x95] = []([[maybe_unused]] CPU& cpu) { return StZeropageReg(cpu, &Registers::A, &Registers::X); };
		s_OpCodes[0x98] = []([[maybe_unused]] CPU& cpu) { return Transfer(&Registers::Y, &Registers::A); };
		s_OpCodes[0x99] = []([[maybe_unused]] CPU& cpu) { return StaAbsoluteReg(cpu, &Registers::Y); };
		s_OpCodes[0x9a] = []([[maybe_unused]] CPU& cpu) { s_Registers.SP = s_Registers.X; return OpValue{ 1, 2 }; };
		s_OpCodes[0x9d] = []([[maybe_unused]] CPU& cpu) { return StaAbsoluteReg(cpu, &Registers::X); };
		s_OpCodes[0xa0] = []([[maybe_unused]] CPU& cpu) { return LdImmediate(cpu, &Registers::Y); };
		s_OpCodes[0xa2] = []([[maybe_unused]] CPU& cpu) { return LdImmediate(cpu, &Registers::X); };
		s_OpCodes[0xa4] = []([[maybe_unused]] CPU& cpu) { return LdZeropage(cpu, &Registers::Y); };
		s_OpCodes[0xa5] = []([[maybe_unused]] CPU& cpu) { return LdZeropage(cpu, &Registers::A); };
		s_OpCodes[0xa6] = []([[maybe_unused]] CPU& cpu) { return LdZeropage(cpu, &Registers::X); };
		s_OpCodes[0xa8] = []([[maybe_unused]] CPU& cpu) { return Transfer(&Registers::A, &Registers::Y); };
		s_OpCodes[0xa9] = []([[maybe_unused]] CPU& cpu) { return LdImmediate(cpu, &Registers::A); };
		s_OpCodes[0xaa] = []([[maybe_unused]] CPU& cpu) { return Transfer(&Registers::A, &Registers::X); };
		s_OpCodes[0xac] = []([[maybe_unused]] CPU& cpu) { return LdAbsolute(cpu, &Registers::Y); };
		s_OpCodes[0xad] = []([[maybe_unused]] CPU& cpu) { return LdAbsolute(cpu, &Registers::A); };
		s_OpCodes[0xae] = []([[maybe_unused]] CPU& cpu) { return LdAbsolute(cpu, &Registers::X); };
		s_OpCodes[0xb0] = []([[maybe_unused]] CPU& cpu) { return Branch(cpu, FlagCarry, true); };
		s_OpCodes[0xb1] = []([[maybe_unused]] CPU& cpu) { return LdaIndirectIndex(cpu); };
		s_OpCodes[0xb4] = []([[maybe_unused]] CPU& cpu) { return LdZeropageReg(cpu, &Registers::Y, &Registers::X); };
		s_OpCodes[0xb5] = []([[maybe_unused]] CPU& cpu) { return LdZeropageReg(cpu, &Registers::A, &Registers::X); };
		s_OpCodes[0xb6] = []([[maybe_unused]] CPU& cpu) { return LdZeropageReg(cpu, &Registers::X, &Registers::Y); };
		s_OpCodes[0xbd] = []([[maybe_unused]] CPU& cpu) { return LdAbsoluteReg(cpu, &Registers::A, &Registers::X); };
		s_OpCodes[0xbe] = []([[maybe_unused]] CPU& cpu) { return LdAbsoluteReg(cpu, &Registers::X, &Registers::Y); };
		s_OpCodes[0xb9] = []([[maybe_unused]] CPU& cpu) { return LdAbsoluteReg(cpu, &Registers::A, &Registers::Y); };
		s_OpCodes[0xbc] = []([[maybe_unused]] CPU& cpu) { return LdAbsoluteReg(cpu, &Registers::Y, &Registers::X); };
		s_OpCodes[0xc0] = []([[maybe_unused]] CPU& cpu) { return CmpImmediate(cpu, &Registers::Y); };
		s_OpCodes[0xc5] = []([[maybe_unused]] CPU& cpu) { return CmpZeropage(cpu, &Registers::A); };
		s_OpCodes[0xc6] = []([[maybe_unused]] CPU& cpu) { return DecZeropage(cpu); };
		s_OpCodes[0xc8] = []([[maybe_unused]] CPU& cpu) { return Inc(&Registers::Y); };
		s_OpCodes[0xc9] = []([[maybe_unused]] CPU& cpu) { return CmpImmediate(cpu, &Registers::A); };
		s_OpCodes[0xca] = []([[maybe_unused]] CPU& cpu) { return Dec(&Registers::X); };
		s_OpCodes[0xcc] = []([[maybe_unused]] CPU& cpu) { return CmpAbsolute(cpu, &Registers::Y); };
		s_OpCodes[0xcd] = []([[maybe_unused]] CPU& cpu) { return CmpAbsolute(cpu, &Registers::A); };
		s_OpCodes[0xce] = []([[maybe_unused]] CPU& cpu) { return DecAbsolute(cpu); };
		s_OpCodes[0xd0] = []([[maybe_unused]] CPU& cpu) { return Branch(cpu, FlagZero, false); };
		s_OpCodes[0xd5] = []([[maybe_unused]] CPU& cpu) { return CmpZeropageReg(cpu, &Registers::A, &Registers::X); };
		s_OpCodes[0xd6] = []([[maybe_unused]] CPU& cpu) { return DecZeropageReg(cpu, &Registers::X); };
		s_OpCodes[0xd8] = []([[maybe_unused]] CPU& cpu) { s_Flags[FlagDecimal] = false; return OpValue{ 1, 2 }; };
		s_OpCodes[0xd9] = []([[maybe_unused]] CPU& cpu) { return CmpAbsoluteIndexed(cpu, &Registers::A, &Registers::Y); };
		s_OpCodes[0xde] = []([[maybe_unused]] CPU& cpu) { return DecAbsoluteRegister(cpu, &Registers::X); };
		s_OpCodes[0xe0] = []([[maybe_unused]] CPU& cpu) { return CmpImmediate(cpu, &Registers::X); };
		s_OpCodes[0xe5] = []([[maybe_unused]] CPU& cpu) { return SbcZeropage(cpu); };
		s_OpCodes[0xe6] = []([[maybe_unused]] CPU& cpu) { return IncZeropage(cpu); };
		s_OpCodes[0xe8] = []([[maybe_unused]] CPU& cpu) { return Inc(&Registers::X); };
		s_OpCodes[0xe9] = []([[maybe_unused]] CPU& cpu) { return SbcImmediate(cpu); };
		s_OpCodes[0xed] = []([[maybe_unused]] CPU& cpu) { return SbcAbsolute(cpu); };
		s_OpCodes[0xee] = []([[maybe_unused]] CPU& cpu) { return IncAbsolute(cpu); };
		s_OpCodes[0xf0] = []([[maybe_unused]] CPU& cpu) { return Branch(cpu, FlagZero, true); };
		s_OpCodes[0xf5] = []([[maybe_unused]] CPU& cpu) { return SbcZeropageOffset(cpu, &Registers::X); };
		s_OpCodes[0xf9] = []([[maybe_unused]] CPU& cpu) { return SbcAbsoluteOffset(cpu, &Registers::Y); };
	}


	CPU::CPU(PowerHandler& powerHandler, MemoryManager& memoryManager)
		: m_PowerHandler(powerHandler), m_MemoryManager(memoryManager)
	{
		for (auto& fn : s_OpCodes)
		{
			fn = [](CPU& cpu) { return NOP(cpu); };
		}

		SetupOpCodeArray();

		{
			MemoryChunk chunk{};
			chunk.StartAddress = 0x0000;
			chunk.Size = 0x8000;
			chunk.Type = MemoryType::RAM;
			chunk.Owner = MemoryOwner::CPU;
			chunk.Name = "CPU RAM";

			m_MemoryManager.AddChunk(chunk);
		}
	}


	auto CPU::Execute(std::uint16_t startVector) -> void
	{
		m_Executing.store(true);

		s_Flags[FlagInterrupt] = false;

		if (startVector == 0)
		{
			auto resetVector = 0xFFFC;
			s_Registers.PC = m_MemoryManager.ReadMemory(MemoryOwner::CPU, resetVector + 1) << 8 + m_MemoryManager.ReadMemory(MemoryOwner::CPU, resetVector);
		}
		else
			s_Registers.PC = startVector;

		// Sets VBLANK flag - keep it static for debugging until PPU is implemented :-)
//		m_Memory.Write(0x2002, 0x80);

		const auto frequency = s_Frequency[FrequencyType::NTSC];
		const auto cycleTime = 1'000'000'000 / frequency;

		LARGE_INTEGER li{};
		if (!QueryPerformanceFrequency(&li))
		{
			std::println("QueryPerformanceFrequency failed");
		}

		auto freq = li.QuadPart; //  / 1'000.0;
		auto frequencyDivider = static_cast<double>(freq) / frequency;

		std::uint64_t cycles{ 0 };

		
		while (m_Executing.load())
		{
			if (m_PowerHandler.GetState() == PowerState::SingleStep || m_PowerHandler.GetState() == PowerState::Off)
				m_PowerHandler.SetState(PowerState::Suspended);
			{
				std::unique_lock<std::mutex> lock(m_Mutex);
				m_CV.wait(lock, [&] { 
					return m_PowerHandler.GetState() != PowerState::Suspended || m_Executing.load() == 0;
				});
			}

			{
				m_MemoryManager.WriteMemory(MemoryOwner::CPU, 0x4016, Controller::GetButtonBits());
			}

// Debug Vframe when PPU is disable
//			m_MemoryManager.WriteMemory(MemoryOwner::CPU, 0x2002, 0x80);

			{
				LARGE_INTEGER startCount{};
				QueryPerformanceCounter(&startCount);

				uint16_t frameCycles{ 0 };

				if (s_NMI.load() && s_NMIRunning.load() == false && cycles > 500)
				{
					s_NMIRunning.store(true);
					Break(*this);
					// PC = 0xFFFA - 1
					s_Registers.PC = 0xFFF9;

//					m_PowerHandler.SetState(PowerState::SingleStep);

					JmpAbsolute(*this);
				}
				else if (s_IRQ.load() && !s_Flags[FlagInterrupt])
				{
					s_IRQ.store(false);
					std::println("IRQ vector is unused in NES");
				}

				s_NMI.store(false);

				auto opCode = m_MemoryManager.ReadMemory(MemoryOwner::CPU, s_Registers.PC);
				auto maybeExecuted = s_OpCodes[opCode](*this);

				if (!maybeExecuted)
					break;

				if (maybeExecuted->ClockCycles == 0)
				{
					std::println("Invalid opcode: {:02x}", opCode);
//					m_RunningMode.store(RunningMode::Halt);
					m_PowerHandler.SetState(PowerState::Suspended);
				}

				s_Registers.PC += maybeExecuted->Size;

//				if (opCode == 0x60)
//				if (s_Registers.PC == 0x8e04)
//				if (s_Registers.PC == 0x9012)
//				if (s_Registers.PC == 0x8ebb)
//				if (s_Registers.PC == 0x8e5c)
//					m_PowerHandler.SetState(PowerState::Suspended);
				if (s_StepToRTS.load() && opCode == 0x60)
				{
					m_PowerHandler.SetState(PowerState::Suspended);
					s_StepToRTS.store(false);
				}

				cycles += maybeExecuted->ClockCycles + s_DMACycles;
				frameCycles += maybeExecuted->ClockCycles + s_DMACycles;

				s_DMACycles = 0;

				std::int64_t countsElapsed{};

				// Cycle sync
				do
				{
					LARGE_INTEGER endCount;
					QueryPerformanceCounter(&endCount);

					countsElapsed = endCount.QuadPart - startCount.QuadPart;
				} while (static_cast<double>(countsElapsed) < (frameCycles * frequencyDivider));
			}
		}

		std::println("Cycles: {}", cycles);
	}

	auto CPU::GetRegisters() -> Registers&
	{
		return s_Registers;
	}

	auto CPU::GetFlags() -> const std::uint8_t
	{
		return static_cast<std::uint8_t>(s_Flags.to_ulong());
	}

	auto CPU::Stop() -> void
	{
		m_Executing.store(false);
		m_CV.notify_all();
	}

	auto CPU::UpdatePowerState() -> void
	{
		m_CV.notify_all();
	}

	auto CPU::TriggerNMI() -> void
	{
		if (s_NMIRunning.load() == true)
			return;

		s_NMI.store(true);
	}

	auto CPU::StepToRTS() -> void
	{
		s_StepToRTS.store(true);
	}

}
