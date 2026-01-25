#include <gtest/gtest.h>

#include "emu/cpu6502/cpu.h"
#include <print>
#include <vector>


TEST(CpuTests, ImmediateAddressing_LDA)
{
	emu::Memory memory;
	std::vector<uint8_t> program{ 0xA9, 0xCD };
	memory.InstallROM(0x1000, program);

	emu::CPU cpu(memory);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(registers.A, 0xCD);
}

TEST(CpuTests, ImmediateAddressing_LDX)
{
	emu::Memory memory;
	std::vector<uint8_t> program{ 0xA2, 0xCD };
	memory.InstallROM(0x1000, program);

	emu::CPU cpu(memory);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(registers.X, 0xCD);
}

TEST(CpuTests, ImmediateAddressing_LDY)
{
	emu::Memory memory;
	std::vector<uint8_t> program{ 0xA0, 0xCD };
	memory.InstallROM(0x1000, program);

	emu::CPU cpu(memory);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(registers.Y, 0xCD);
}
