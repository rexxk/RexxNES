#include "emu/cpu6502/cpu.h"
#include "emu/memory/memory.h"

#include <cstdint>
#include <fstream>
#include <print>
#include <thread>
#include <vector>


struct ROMHeader
{
	std::uint8_t Magic[4]{};
	std::uint8_t PrgRomSize{};
	std::uint8_t ChrRomSize{};
	std::uint8_t Flags1{};
	std::uint8_t Flags2{};
	std::uint8_t Flags3{};
	std::uint8_t Flags4{};
	std::uint8_t Flags5{};
	std::uint8_t Reserved[5]{};
};


auto main() -> int
{
	std::println("RexxNES 2026 - emulation at its worst");

	std::vector<std::uint8_t> program
	{
		0xA9, 0x10, 0x85, 0x02, 0xAA, 0xC8, 0x88, 0xC9, 
		0xAB,
	};

	std::ifstream fs("rom/SuperMarioBros.nes", std::ios::in | std::ios::binary);

	if (!fs.is_open())
	{
		std::println("Failed to open rom file");
		return -1;
	}

	fs.seekg(0, fs.end);
	std::size_t romSize = fs.tellg();
	fs.seekg(0, fs.beg);

//	std::println("ROM size: {} bytes", romSize);

	ROMHeader romHeader{};
	fs.read((char*)&romHeader, sizeof(ROMHeader));

	if (romHeader.Flags1 & 0x04)
	{
		std::vector<uint8_t> trainerData(512);
		std::println("512 bytes trainer is present");
		fs.read((char*)trainerData.data(), 512);
	}

//	std::println("Reading prog ROM");

	std::vector<uint8_t> programRom(romHeader.PrgRomSize * 0x4000);
	fs.read((char*)programRom.data(), romHeader.PrgRomSize * 0x4000);

//	std::println("Reading char ROM");

	std::vector<uint8_t> charRom(romHeader.ChrRomSize * 0x2000);
	fs.read((char*)charRom.data(), romHeader.ChrRomSize * 0x2000);

//	std::println("File location: {}", static_cast<std::size_t>(fs.tellg()));

	//std::uint8_t Flags1{};
	//std::uint8_t Flags2{};
	//std::uint8_t Flags3{};
	//std::uint8_t Flags4{};
	//std::uint8_t Flags5{};
//	std::println(" * Program ROM size : {} bytes", romHeader.PrgRomSize * 0x4000);
//	std::println(" * Char ROM size    : {} bytes", romHeader.ChrRomSize * 0x2000);
//
//	std::println(" * Flags 6          : {:08b}", romHeader.Flags1);
//	std::println(" * Flags 7          : {:08b}", romHeader.Flags2);
//	std::println(" * Flags 8          : {:08b}", romHeader.Flags3);
//	std::println(" * Flags 9          : {:08b}", romHeader.Flags4);
//	std::println(" * Flags 10         : {:08b}", romHeader.Flags5);

	fs.close();

	std::uint8_t mapperID = (romHeader.Flags2 & 0xF0) + ((romHeader.Flags1 & 0xF0) >> 4);

	std::println("Mapper ID#: {}", mapperID);

	emu::Memory cpuMemory{};
	cpuMemory.InstallROM(0x8000, programRom);
	cpuMemory.InstallROM(0x0000, charRom);

//	emu::Memory ppuMemory{};
//	ppuMemory.InstallROM(0x0000, charRom);

	emu::CPU cpu{cpuMemory};

	std::thread cpuThread(&emu::CPU::Execute, &cpu, 0);
//	cpu.Execute();

	cpuThread.join();
//	cpu.Execute(program, 0x1000);

	return 0;
}
