#include <gtest/gtest.h>

#include "emu/cpu6502/cpu.h"
#include "emu/memory/dma.h"
#include <print>
#include <vector>


TEST(CpuTests, LDA_ImmediateAddressing)
{
	emu::Memory memory(16);
	std::vector<uint8_t> program{ 0xA9, 0xCD };
	memory.InstallROM(0x1000, program);

	emu::DMA oamDMA{ 0x4014, memory, memory };
	emu::DMA dmcDMA{ 0x4015, memory, memory };
	emu::CPU cpu(memory, oamDMA, dmcDMA);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(registers.A, 0xCD);
}

TEST(CpuTests, LDX_ImmediateAddressing)
{
	emu::Memory memory{ 16 };
	std::vector<uint8_t> program{ 0xA2, 0xCD };
	memory.InstallROM(0x1000, program);

	emu::DMA oamDMA{ 0x4014, memory, memory };
	emu::DMA dmcDMA{ 0x4015, memory, memory };
	emu::CPU cpu(memory, oamDMA, dmcDMA);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(registers.X, 0xCD);
}

TEST(CpuTests, LDY_ImmediateAddressing)
{
	emu::Memory memory{ 16 };
	std::vector<uint8_t> program{ 0xA0, 0xCD };
	memory.InstallROM(0x1000, program);

	emu::DMA oamDMA{ 0x4014, memory, memory };
	emu::DMA dmcDMA{ 0x4015, memory, memory };
	emu::CPU cpu(memory, oamDMA, dmcDMA);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(registers.Y, 0xCD);
}


TEST(CpuTests, LDA_AbsoluteAddressing)
{
	emu::Memory memory{ 16 };
	std::vector<uint8_t> program{ 0xAD, 0x03, 0x10, 0x35 };
	memory.InstallROM(0x1000, program);

	emu::DMA oamDMA{ 0x4014, memory, memory };
	emu::DMA dmcDMA{ 0x4015, memory, memory };
	emu::CPU cpu(memory, oamDMA, dmcDMA);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(registers.A, 0x35);
}

TEST(CpuTests, LDX_AbsoluteAddressing)
{
	emu::Memory memory{ 16 };
	std::vector<uint8_t> program{ 0xAE, 0x03, 0x10, 0x35 };
	memory.InstallROM(0x1000, program);

	emu::DMA oamDMA{ 0x4014, memory, memory };
	emu::DMA dmcDMA{ 0x4015, memory, memory };
	emu::CPU cpu(memory, oamDMA, dmcDMA);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(registers.X, 0x35);
}

TEST(CpuTests, LDY_AbsoluteAddressing)
{
	emu::Memory memory{ 16 };
	std::vector<uint8_t> program{ 0xAC, 0x03, 0x10, 0x35 };
	memory.InstallROM(0x1000, program);

	emu::DMA oamDMA{ 0x4014, memory, memory };
	emu::DMA dmcDMA{ 0x4015, memory, memory };
	emu::CPU cpu(memory, oamDMA, dmcDMA);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(registers.Y, 0x35);
}


TEST(CpuTests, LDA_AbsoluteAddressingOffset)
{
	emu::Memory memory{ 16 };
	std::vector<uint8_t> program{ 0xA2, 0x01, 0xBD, 0x05, 0x10, 0x35, 0x17 };
	memory.InstallROM(0x1000, program);

	emu::DMA oamDMA{ 0x4014, memory, memory };
	emu::DMA dmcDMA{ 0x4015, memory, memory };
	emu::CPU cpu(memory, oamDMA, dmcDMA);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(registers.A, 0x17);
}



TEST(CpuTests, STA_Zeropage)
{
	emu::Memory memory{ 16 };
	std::vector<uint8_t> program{ 0xA9, 0x25, 0x85, 0x02 };
	memory.InstallROM(0x1000, program);

	emu::DMA oamDMA{ 0x4014, memory, memory };
	emu::DMA dmcDMA{ 0x4015, memory, memory };
	emu::CPU cpu(memory, oamDMA, dmcDMA);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(memory.Read(0x0002), 0x25);
}

TEST(CpuTests, STX_Zeropage)
{
	emu::Memory memory{ 16 };
	std::vector<uint8_t> program{ 0xA2, 0x31, 0x86, 0xc2 };
	memory.InstallROM(0x1000, program);

	emu::DMA oamDMA{ 0x4014, memory, memory };
	emu::DMA dmcDMA{ 0x4015, memory, memory };
	emu::CPU cpu(memory, oamDMA, dmcDMA);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(memory.Read(0x00c2), 0x31);
}

TEST(CpuTests, STA_IndirectIndexed)
{
	emu::Memory memory{ 16 };
	memory.Write(0x02, 0x00);
	memory.Write(0x03, 0x01);
	std::vector<uint8_t> program{ 0xA9, 0x12, 0xA0, 0x03, 0x91, 0x02 };
	memory.InstallROM(0x1000, program);

	emu::DMA oamDMA{ 0x4014, memory, memory };
	emu::DMA dmcDMA{ 0x4015, memory, memory };
	emu::CPU cpu(memory, oamDMA, dmcDMA);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(memory.Read(0x0103), 0x12);
}


TEST(CpuTests, DEY_Implied)
{
	emu::Memory memory{ 16 };
	std::vector<uint8_t> program{ 0xA0, 0x10, 0x88 };
	memory.InstallROM(0x1000, program);

	emu::DMA oamDMA{ 0x4014, memory, memory };
	emu::DMA dmcDMA{ 0x4015, memory, memory };
	emu::CPU cpu(memory, oamDMA, dmcDMA);

	cpu.Execute(0x1000);
	auto& registers = cpu.GetRegisters();

	ASSERT_EQ(registers.Y, 0x0F);
}


TEST(CpuTests, CMP_Immediate)
{
	emu::Memory memory{ 16 };
	emu::DMA oamDMA{ 0x4014, memory, memory };
	emu::DMA dmcDMA{ 0x4015, memory, memory };
	emu::CPU cpu(memory, oamDMA, dmcDMA);

	{
		std::vector<uint8_t> program{ 0xA9, 0x45, 0xC9, 0x85 };
		memory.InstallROM(0x1000, program);


		cpu.Execute(0x1000);
		auto flags = cpu.GetFlags();

		ASSERT_EQ((flags & 0b1000'0000) == 0x80, true);			// Negative
		ASSERT_EQ((flags & 0b0000'0010) == 0x02, false);		// Zero
		ASSERT_EQ((flags & 0b0000'0001) == 0x01, true);			// Carry
	}

	{
		std::vector<uint8_t> program{ 0xA9, 0x45, 0xC9, 0x28 };
		memory.InstallROM(0x1000, program);

		cpu.Execute(0x1000);
		auto flags = cpu.GetFlags();

		ASSERT_EQ((flags & 0b1000'0000) == 0x80, false);		// Negative
		ASSERT_EQ((flags & 0b0000'0010) == 0x02, false);		// Zero
		ASSERT_EQ((flags & 0b0000'0001) == 0x01, true);			// Carry
	}

	{
		std::vector<uint8_t> program{ 0xA9, 0x45, 0xC9, 0x45 };
		memory.InstallROM(0x1000, program);

		cpu.Execute(0x1000);
		auto flags = cpu.GetFlags();

		ASSERT_EQ((flags & 0b1000'0000) == 0x80, false);		// Negative
		ASSERT_EQ((flags & 0b0000'0010) == 0x02, true);			// Zero
		ASSERT_EQ((flags & 0b0000'0001) == 0x01, true);			// Carry
	}
}
