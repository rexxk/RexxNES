#include <gtest/gtest.h>

#include "emu/cpu6502/cpu.h"
#include <print>
#include <vector>


TEST(CpuTests, LDA_ImmediateAddressing)
{
	emu::Memory memory;
	std::vector<uint8_t> program{ 0xA9, 0xCD };
	memory.InstallROM(0x1000, program);

	emu::CPU cpu(memory);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(registers.A, 0xCD);
}

TEST(CpuTests, LDX_ImmediateAddressing)
{
	emu::Memory memory;
	std::vector<uint8_t> program{ 0xA2, 0xCD };
	memory.InstallROM(0x1000, program);

	emu::CPU cpu(memory);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(registers.X, 0xCD);
}

TEST(CpuTests, LDY_ImmediateAddressing)
{
	emu::Memory memory;
	std::vector<uint8_t> program{ 0xA0, 0xCD };
	memory.InstallROM(0x1000, program);

	emu::CPU cpu(memory);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(registers.Y, 0xCD);
}


TEST(CpuTests, LDA_AbsoluteAddressing)
{
	emu::Memory memory;
	std::vector<uint8_t> program{ 0xAD, 0x03, 0x10, 0x35 };
	memory.InstallROM(0x1000, program);

	emu::CPU cpu(memory);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(registers.A, 0x35);
}

TEST(CpuTests, LDX_AbsoluteAddressing)
{
	emu::Memory memory;
	std::vector<uint8_t> program{ 0xAE, 0x03, 0x10, 0x35 };
	memory.InstallROM(0x1000, program);

	emu::CPU cpu(memory);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(registers.X, 0x35);
}

TEST(CpuTests, LDY_AbsoluteAddressing)
{
	emu::Memory memory;
	std::vector<uint8_t> program{ 0xAC, 0x03, 0x10, 0x35 };
	memory.InstallROM(0x1000, program);

	emu::CPU cpu(memory);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(registers.Y, 0x35);
}


TEST(CpuTests, LDA_AbsoluteAddressingOffset)
{
	emu::Memory memory;
	std::vector<uint8_t> program{ 0xA2, 0x01, 0xBD, 0x05, 0x10, 0x35, 0x17 };
	memory.InstallROM(0x1000, program);

	emu::CPU cpu(memory);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(registers.A, 0x17);
}



TEST(CpuTests, STA_Zeropage)
{
	emu::Memory memory;
	std::vector<uint8_t> program{ 0xA9, 0x25, 0x85, 0x02 };
	memory.InstallROM(0x1000, program);

	emu::CPU cpu(memory);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(memory.Read(0x0002), 0x25);
}

TEST(CpuTests, STX_Zeropage)
{
	emu::Memory memory;
	std::vector<uint8_t> program{ 0xA2, 0x31, 0x86, 0xc2 };
	memory.InstallROM(0x1000, program);

	emu::CPU cpu(memory);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(memory.Read(0x00c2), 0x31);
}

TEST(CpuTests, STA_IndirectIndexed)
{
	emu::Memory memory;
	memory.Write(0x02, 0x00);
	memory.Write(0x03, 0x01);
	std::vector<uint8_t> program{ 0xA9, 0x12, 0xA0, 0x03, 0x91, 0x02 };
	memory.InstallROM(0x1000, program);

	emu::CPU cpu(memory);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(memory.Read(0x0103), 0x12);
}


TEST(CpuTests, DEY_implied)
{
	emu::Memory memory;
	std::vector<uint8_t> program{ 0xA0, 0x10, 0x88 };
	memory.InstallROM(0x1000, program);

	emu::CPU cpu(memory);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(registers.Y, 0x0F);
}
