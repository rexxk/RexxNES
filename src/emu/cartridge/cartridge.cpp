#include "emu/cartridge/cartridge.h"

#include <cstdint>
#include <fstream>
#include <print>
#include <vector>


namespace emu
{

	struct iNESHeader
	{
		std::uint8_t Signature[4];
		std::uint8_t ProgramROMSize;
		std::uint8_t CharROMSize;
		std::uint8_t Flags6;
		std::uint8_t Flags7;
		std::uint8_t Flags8;
		std::uint8_t Flags9;
		std::uint8_t Flags10;
		std::uint8_t Padding[5];
	};

	static iNESHeader Header{};
	static std::uint8_t MapperNumber{};
	static std::uint8_t NametableMirroring{};
	static std::uint8_t AlternativeNametableLayout{};
	static bool ContainsPRGRAM{};
	static bool ContainsTrainer{};
	static bool NES2Format{};
	static std::uint8_t TVSystem{};

	auto ParseHeader() -> void
	{
		if (std::strncmp((char*)&Header, "NES\x{1a}", 4))
		{
			std::println("Invalid cartridge file detected. (Signature: {:c}{:c}{:c}{:c})", Header.Signature[0], Header.Signature[1], Header.Signature[2], Header.Signature[3]);
			return;
		}

		Header.ProgramROMSize *= 16;
		Header.CharROMSize *= 8;

		std::println(" * Program ROM size (KB) : {}", Header.ProgramROMSize);
		std::println(" * Char ROM size (KB)    : {}", Header.CharROMSize);

		NametableMirroring = Header.Flags6 & 0x01;
		AlternativeNametableLayout = Header.Flags6 & 0x08;
		ContainsTrainer = Header.Flags6 & 0x04;
		ContainsPRGRAM = Header.Flags6 & 0x02;
		MapperNumber = (Header.Flags7) & 0xF0;
		MapperNumber += (Header.Flags6 & 0xF0) >> 4;

		std::println(" * Nametable mirror      : {}", NametableMirroring);
		std::println(" * Alt. nametable layout : {}", AlternativeNametableLayout);
		std::println(" * Mapper number         : {}", MapperNumber);
		std::println(" * Contains trainer      : {}", ContainsTrainer);
		std::println(" * Contains PGM RAM      : {}", ContainsPRGRAM);

		NES2Format = Header.Flags7 & 0x40;

		std::println(" * NES 2.0 format        : {}", NES2Format);

		TVSystem = Header.Flags9 & 0x01;

		std::println(" * TV system             : {}", TVSystem);
	}


	Cartridge::Cartridge(const std::filesystem::path& filePath)
	{
		std::ifstream fs(filePath, std::ios::in | std::ios::binary);

		if (!fs.is_open())
		{
			std::println("Failed to load cartridge: {}", filePath.filename().string());
			return;
		}

		std::println("Cartridge installed: {}", filePath.filename().string());

		fs.seekg(0, fs.end);
		auto filesize = fs.tellg();
		fs.seekg(0, fs.beg);

		fs.read((char*)&Header, sizeof(iNESHeader));

		fs.close();

		ParseHeader();


	}


	Cartridge::~Cartridge()
	{

	}

}
