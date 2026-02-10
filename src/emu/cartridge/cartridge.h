#pragma once

#include "emu/memory/rom.h"

#include <cstdint>
#include <filesystem>
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

	private:
		ROM m_ProgramROM{};
		ROM m_CharROM{};

		std::vector<std::uint8_t> m_Trainer{};

	};


}
