#include "emu/memory/memorymanager.h"

#include "emu/cartridge/mapper.h"
#include "emu/cpu6502/cpu.h"
#include "input/controller.h"

#include <mutex>
#include <print>

#include "imgui.h"



namespace emu
{


	static int SelectedMemory{ 0 };
	static int MemoryPage{ 0 };

	static std::uint16_t PPUAddress{ 0u };
	static std::uint16_t OAMAddress{ 0u };
	static bool RegisterW{ false };

	static std::uint16_t RegisterV{ 0u };
	static std::uint16_t RegisterT{ 0u };
	static std::uint8_t RegisterX{ 0u };

	static std::uint16_t ScrollX{ 0u };
	static std::uint16_t ScrollY{ 0u };

	static std::uint8_t ControllerClock{ 0u };
	static std::uint8_t PPUDataBuffer{ 0u };

	static MemoryMap Map;


	auto ReadController(std::uint8_t controllerID) -> std::uint8_t 
	{
		// TODO: Implement controller ID so that two controllers can be used. Controller 1 is hardcoded.
		if (controllerID == 1)
			return 0;

		auto bits = Controller::GetData();

		std::uint8_t value = ((bits >> ControllerClock++) & 0x01);

		if (ControllerClock > 7)
			ControllerClock = 0;

		return value;
	}


	MemoryManager::MemoryManager(Cartridge& cartridge)
		: m_Cartridge(cartridge)
	{
		Map = Mapper::CreateMemoryMap(cartridge);
	}

	MemoryManager::~MemoryManager()
	{

	}

	auto MemoryManager::ClearPPUIOBit(std::uint16_t address, std::uint8_t bit) -> void
	{
		Map.PPUIO.Data.at(address - Map.PPUIO.StartAddress) &= ~bit;
	}

	auto MemoryManager::GetPPUIOBit(std::uint16_t address) -> std::uint8_t
	{
		return Map.PPUIO.Data.at(address - Map.PPUIO.StartAddress);
	}

	auto MemoryManager::SetPPUIOBit(std::uint16_t address, std::uint8_t bit) -> void
	{
		Map.PPUIO.Data.at(address - Map.PPUIO.StartAddress) |= bit;
	}

	auto MemoryManager::ReadCharROM(std::uint16_t address) -> std::uint8_t
	{
		return Map.CharROM.Data.at(address - Map.CharROM.StartAddress);
	}

	auto MemoryManager::ReadProgramROM(std::uint16_t address) -> std::uint8_t
	{
		return Map.ProgramROM.Data.at(address - Map.ProgramROM.StartAddress);
	}

	auto MemoryManager::ReadAPURAM(std::uint16_t address) -> std::uint8_t
	{
		return Map.APURAM.Data.at(address - Map.APURAM.StartAddress);
	}

	auto MemoryManager::ReadCPURAM(std::uint16_t address) -> std::uint8_t
	{
		return Map.CPURAM.Data.at(address - Map.CPURAM.StartAddress);
	}

	auto MemoryManager::ReadOAMRAM(std::uint16_t address) -> std::uint8_t
	{
		return Map.OAMRAM.Data.at(address - Map.OAMRAM.StartAddress);
	}

	auto MemoryManager::ReadPPURAM(std::uint16_t address) -> std::uint8_t
	{
		std::lock_guard<std::mutex> lock(m_PPURAMMutex);

		return Map.PPURAM.Data.at(address - Map.PPURAM.StartAddress);
	}
	
	auto MemoryManager::ReadAPUIO(std::uint16_t address) -> std::uint8_t
	{
		if (address == 0x4016)
		{
			auto value = ReadController(0);
			Map.APUIO.Data.at(address - Map.APUIO.StartAddress) = value;
//			Map.APUIO.Data.at(address - Map.APUIO.StartAddress) = Controller::GetData();
//			return ReadController(0);
		}

		if (address == 0x4017)
		{
			auto value = ReadController(1);
			Map.APUIO.Data.at(address - Map.APUIO.StartAddress) = value;
//			return ReadController(1);
		}

		return Map.APUIO.Data.at(address - Map.APUIO.StartAddress);
	}

	auto MemoryManager::ReadPPUIO(std::uint16_t address) -> std::uint8_t
	{
		if (address >= 0x2002 && address < 0x2008)
		{
			if (address == 0x2002)
			{
				auto value = Map.PPUIO.Data.at(2);
				Map.PPUIO.Data.at(2) &= 0x7F;
				RegisterW = false;

				return value;
			}

			if (address == 0x2007)
			{
				std::uint8_t value = PPUDataBuffer;

				if (PPUAddress >= 0x2000)
					PPUDataBuffer = ReadPPURAM(PPUAddress);
				else
					PPUDataBuffer = ReadCharROM(PPUAddress);

				PPUAddress += ReadPPUIO(0x2000) & 0x4 ? 32 : 1;

				return value;
			}
		}

		return Map.PPUIO.Data.at(address - Map.PPUIO.StartAddress);
	}

	auto MemoryManager::WriteAPURAM(std::uint16_t address, std::uint8_t value) -> void
	{
		Map.APURAM.Data.at(address - Map.APURAM.StartAddress) = value;
	}

	auto MemoryManager::WriteCPURAM(std::uint16_t address, std::uint8_t value) -> void
	{
		Map.CPURAM.Data.at(address - Map.CPURAM.StartAddress) = value;
	}

	auto MemoryManager::WriteOAMRAM(std::uint16_t address, std::uint8_t value) -> void
	{
		Map.OAMRAM.Data.at(address - Map.OAMRAM.StartAddress) = value;
	}

	auto MemoryManager::WritePPURAM(std::uint16_t address, std::uint8_t value) -> void
	{
		std::lock_guard<std::mutex> lock(m_PPURAMMutex);

		if (address >= 0x2000 && address < 0x3000)
		{
			if (m_Cartridge.GetAttributes().NametableMirroring == 0)
				Map.PPURAM.Data.at((address + 0x400) - Map.PPURAM.StartAddress) = value;
			else
				Map.PPURAM.Data.at((address + 0x800) - Map.PPURAM.StartAddress) = value;
		}

		Map.PPURAM.Data.at(address - Map.PPURAM.StartAddress) = value;
	}

	auto MemoryManager::WriteAPUIO(std::uint16_t address, std::uint8_t value) -> void
	{
		std::lock_guard<std::mutex> lock(m_WriteMutex);
		Map.APUIO.Data.at(address - Map.APUIO.StartAddress) = value;

		if (address == 0x4016)
		{
			if (value & 0x1)
			{
				Controller::LatchData();
			}
			else if (value == 0)
			{
				ControllerClock = 0;
			}
		}
	}

	auto MemoryManager::WritePPUIO(std::uint16_t address, std::uint8_t value) -> void
	{
		switch (address)
		{
			case 0x2000:
			{
				RegisterT &= 0xF3;
				RegisterT |= (value & 0x3) << 10;

				// Check for Vblank NMI and Vblank status is 1
				if ((value & 0x80) && (Map.PPUIO.Data.at(2) & 0x80))
				{
					CPU::TriggerNMI();
				}

				break;
			}

			case 0x2002:
			{
				RegisterW = false;
				break;
			}

			case 0x2003:
			{
				OAMAddress = value & 0x00FF;
				break;
			}

			case 0x2004:
			{
				WriteOAMRAM(OAMAddress, value);
				OAMAddress++;

				break;
			}

			case 0x2005:
			{
//				if (value != 0)
//					__debugbreak();

				if (!RegisterW)
				{
					ScrollX = value;

					RegisterT &= 0xE0;
					RegisterT |= (value & 0xF8) >> 3;
					RegisterX = value & 0x07;
				}
				else
				{
					ScrollY = value;

					RegisterT &= 0x0C1F;
					RegisterT |= (value & 0xF8) << 2;
					RegisterT |= (value & 0x03) << 13;
				}

				RegisterW = !RegisterW;

				break;
			}

			case 0x2006:
			{
				if (!RegisterW)
				{
					PPUAddress = ((value & 0x3F) << 8) & 0xFF00;

					RegisterT &= 0x7FFF;
				}
				else
				{
					PPUAddress += value;
	
					if (PPUAddress > 0x2000)
						PPUDataBuffer = ReadPPURAM(PPUAddress);
					else
						PPUDataBuffer = 0;

					RegisterV = RegisterT;
				}

				RegisterW = !RegisterW;

				break;
			}

			case 0x2007:
			{
				if (PPUAddress < 0x2000 && PPUAddress >= 0x0100)
				{
					std::println("Invalid PPUAddress for write: {:04x}", PPUAddress);
					return;
				}

				WritePPURAM(PPUAddress, value);
				PPUAddress += ReadPPUIO(0x2000) & 0x4 ? 32 : 1;

				break;
			}
		}

		Map.PPUIO.Data.at(address - Map.PPUIO.StartAddress) = value;
	}

	auto MemoryManager::DMATransfer(MemoryOwner targetOwner, std::uint8_t value) -> void
	{
		std::uint8_t addressHigh{};
		std::uint16_t length{};

		if (targetOwner == MemoryOwner::PPU)
		{
			length = 0x100;

			std::uint16_t address = (value << 8) & 0xFF00;

			for (auto i = 0; i < length; i++)
			{
				// Write via PPUDATA
				WriteOAMRAM(i, ReadCPURAM(address + i));
			}
		}
		else if (targetOwner == MemoryOwner::ASU)
		{
			length = 2;
		}

	}

	auto MemoryManager::GetScrollXRegister() const -> const std::uint16_t
	{
		return ScrollX;
	}

	auto MemoryManager::GetScrollYRegister() const -> const std::uint16_t
	{
		return ScrollY;
	}

	auto MemoryManager::GetVRegister() const -> const std::uint16_t
	{
		return RegisterV;
	}

	auto MemoryManager::GetTRegister() const -> const std::uint16_t
	{
		return RegisterT;
	}

	auto MemoryManager::GetXRegister() const -> const std::uint8_t
	{
		return RegisterX;
	}


	auto ViewPage(std::span<std::uint8_t> memory, std::uint16_t address)
	{
		auto startAddress = MemoryPage * 0x100;

		std::uint16_t count{ 0 };

		for (auto row = 0; row < 16; row++)
		{
			std::string str = std::format("{:04x} : ", row * 16 + startAddress + address);

			for (auto col = 0; col < 16; col++)
			{
				if ((row * 16 + startAddress + col) < memory.size())
					str += std::format("{:02x} ", memory[row * 16 + startAddress + col]);
			}

			ImGui::Text("%s", str.c_str());
		}
	}

	auto DrawMemory(Memory& memory) -> void
	{
		// Chunk info block
		{
			ImGui::Text("Start address: %04x", memory.StartAddress);
			ImGui::Text("Length: %04x", memory.Size);

			ImGui::Separator();

			ImGui::Text("%s", memory.Name.c_str());

			ImGui::Separator();

			if (ImGui::InputInt("Page", &MemoryPage))
			{
				if (MemoryPage < 0 || memory.Size <= 0x100) MemoryPage = 0;
				if (MemoryPage > ((memory.Size / 256) - 1) && memory.Size > 0x100) MemoryPage = (memory.Size / 256) - 1;
			}

			ImGui::Separator();

			ViewPage(memory.Data, memory.StartAddress);
		}

	}

	auto MemoryManager::ViewMemory() -> void
	{
		ImGui::Begin("Memory");

		if (ImGui::InputInt("Memory", &SelectedMemory))
		{
			if (SelectedMemory < 0) SelectedMemory = 0;
			if (SelectedMemory >= 7) SelectedMemory = 7;
		}

		switch (SelectedMemory)
		{
			case 0: DrawMemory(Map.ProgramROM); break;
			case 1: DrawMemory(Map.CharROM); break;
			case 2: DrawMemory(Map.PPUIO); break;
			case 3: DrawMemory(Map.APUIO); break;
			case 4: DrawMemory(Map.APURAM); break;
			case 5: DrawMemory(Map.PPURAM); break;
			case 6: DrawMemory(Map.CPURAM); break;
			case 7: DrawMemory(Map.OAMRAM); break;

			default:
				DrawMemory(Map.CPURAM); break;
		}

		ImGui::End();
	}

}
