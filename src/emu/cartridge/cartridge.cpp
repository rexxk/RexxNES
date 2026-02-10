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
	static CartridgeAttributes s_Attributes{};


	auto ParseHeader() -> void
	{
		if (std::strncmp((char*)&Header, "NES\x{1a}", 4))
		{
			std::println("Invalid cartridge file detected. (Signature: {:c}{:c}{:c}{:c})", Header.Signature[0], Header.Signature[1], Header.Signature[2], Header.Signature[3]);
			return;
		}

		std::println(" * Program ROM size (KB) : {}", Header.ProgramROMSize * 16);
		std::println(" * Char ROM size (KB)    : {}", Header.CharROMSize * 8);

		s_Attributes.NametableMirroring = Header.Flags6 & 0x01;
		s_Attributes.AlternativeNametableLayout = Header.Flags6 & 0x08;
		s_Attributes.ContainsTrainer = Header.Flags6 & 0x04;
		s_Attributes.ContainsPRGRAM = Header.Flags6 & 0x02;
		s_Attributes.MapperNumber = (Header.Flags7) & 0xF0;
		s_Attributes.MapperNumber += (Header.Flags6 & 0xF0) >> 4;

		std::println(" * Nametable mirror      : {}", s_Attributes.NametableMirroring);
		std::println(" * Alt. nametable layout : {}", s_Attributes.AlternativeNametableLayout);
		std::println(" * Mapper number         : {}", s_Attributes.MapperNumber);
		std::println(" * Contains trainer      : {}", s_Attributes.ContainsTrainer);
		std::println(" * Contains PGM RAM      : {}", s_Attributes.ContainsPRGRAM);

		s_Attributes.NES2Format = Header.Flags7 & 0x40;

		std::println(" * NES 2.0 format        : {}", s_Attributes.NES2Format);

		s_Attributes.TVSystem = Header.Flags9 & 0x01;

		std::println(" * TV system             : {}", s_Attributes.TVSystem);
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

		ParseHeader();

		if (s_Attributes.ContainsTrainer)
		{
			m_Trainer.resize(512);
			fs.read((char*)m_Trainer.data(), 512);
		}

		{
			auto romSize{ Header.ProgramROMSize * 0x4000 };
			std::vector<std::uint8_t> programROM(romSize);
			fs.read((char*)programROM.data(), romSize);

			m_ProgramROM.SetData(romSize, programROM);
		}

		{
			auto romSize{ Header.CharROMSize * 0x2000 };
			std::vector<std::uint8_t> charROM(romSize);
			fs.read((char*)charROM.data(), romSize);

			m_CharROM.SetData(romSize, charROM);
		}

		fs.close();

	}


	Cartridge::~Cartridge()
	{

	}

	auto Cartridge::GetAttributes() const -> const CartridgeAttributes&
	{
		return s_Attributes;
	}

}
