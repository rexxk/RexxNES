#include "emu/cartridge/mapper.h"



namespace emu
{


	auto Mapper::CreateMemoryMap(Cartridge& cartridge) -> MemoryMap
	{
		MemoryMap newMap;
		auto& attributes = cartridge.GetAttributes();

		newMap.NametableMirroring = attributes.NametableMirroring;

		newMap.CPURAM.StartAddress = 0x0000;
		newMap.CPURAM.Size = 0x800;
		newMap.CPURAM.Data.resize(newMap.CPURAM.Size);
		newMap.CPURAM.Name = "CPU RAM";

		newMap.PPUIO.StartAddress = 0x2000;
		newMap.PPUIO.Size = 0x8;
		newMap.PPUIO.Data.resize(newMap.PPUIO.Size);
		newMap.PPUIO.Name = "PPU IO";

		newMap.APUIO.StartAddress = 0x4000;
		newMap.APUIO.Size = 0x18;
		newMap.APUIO.Data.resize(newMap.APUIO.Size);
		newMap.APUIO.Name = "APU & IO";

		newMap.PPURAM.StartAddress = 0x2000;
		newMap.PPURAM.Size = 0x2000;
		newMap.PPURAM.Data.resize(newMap.PPURAM.Size);
		newMap.PPURAM.Name = "PPU RAM";

		newMap.OAMRAM.StartAddress = 0x0000;
		newMap.OAMRAM.Size = 0x100;
		newMap.OAMRAM.Data.resize(newMap.OAMRAM.Size);
		newMap.OAMRAM.Name = "OAM";

		newMap.APURAM.StartAddress = 0x0000;
		newMap.APURAM.Size = 0x10;
		newMap.APURAM.Data.resize(newMap.APURAM.Size);
		newMap.APURAM.Name = "APU RAM";

		switch (attributes.MapperNumber)
		{
			case 0:
			{
				newMap.ProgramROM.StartAddress = 0x8000;
				newMap.ProgramROM.Size = cartridge.GetROM(ROMType::Program).GetSize();
				newMap.ProgramROM.Data = cartridge.GetROM(ROMType::Program).GetData();
				newMap.ProgramROM.ReadOnly = true;
				newMap.ProgramROM.Name = "Program ROM";

				newMap.CharROM.StartAddress = 0x0000;
				newMap.CharROM.Size = cartridge.GetROM(ROMType::Character).GetSize();
				newMap.CharROM.Data = cartridge.GetROM(ROMType::Character).GetData();
				newMap.CharROM.ReadOnly = true;
				newMap.CharROM.Name = "Char ROM";

				break;
			}

			default:
				break;
		}

		return newMap;
	}


}
