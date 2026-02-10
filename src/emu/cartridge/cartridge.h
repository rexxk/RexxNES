#pragma once

#include <filesystem>


namespace emu
{


	class Cartridge
	{
	public:
		Cartridge(const std::filesystem::path& filePath);
		~Cartridge();

	};


}
