#pragma once

#include "emu/memory/rom.h"

#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <vector>


namespace emu
{

	struct CartridgeAttributes
	{
		std::uint8_t MapperNumber{};
		std::uint8_t NametableMirroring{};
		std::uint8_t AlternativeNametableLayout{};
		bool ContainsPRGRAM{};
		bool ContainsTrainer{};
		bool NES2Format{};
		std::uint8_t TVSystem{};

	};


	class Cartridge
	{
	public:
		Cartridge(const std::filesystem::path& filePath);
		~Cartridge();

		auto GetAttributes() const -> const CartridgeAttributes&;

		auto GetROM(ROMType type) { return m_ROMs[type]; }
//		auto GetProgramROM() const -> auto { return m_ProgramROM; }
//		auto GetCharROM() const -> auto { return m_CharROM; }

	private:
//		ROM m_ProgramROM{};
//		ROM m_CharROM{};

		std::unordered_map<ROMType, ROM> m_ROMs{};
		std::vector<std::uint8_t> m_Trainer{};

	};


}
