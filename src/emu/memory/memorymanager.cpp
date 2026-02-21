#include "emu/memory/memorymanager.h"
#include "input/controller.h"

#include <print>

#include "imgui.h"



namespace emu
{


	static int SelectedChunk{ 0 };
	static int MemoryPage{ 0 };

	static std::uint16_t PPUAddress{ 0u };
	static std::uint16_t OAMAddress{ 0u };
	static bool RegisterW{ false };

	static std::uint16_t ScrollX{ 0u };
	static std::uint16_t ScrollY{ 0u };

	static std::uint8_t ControllerClock{ 0u };



	auto ReadController(std::uint8_t controllerID) -> std::uint8_t 
	{
		// TODO: Implement controller ID so that two controllers can be used. Controller 1 is hardcoded.
		if (controllerID == 1)
			return 0;

		auto bits = Controller::GetButtonBits();

		std::uint8_t value = (bits >> ControllerClock++) & 0x01;

		if (ControllerClock > 7)
			ControllerClock = 0;

//		if (ControllerClock == 4)
//			return 1;

		return value;
	}




	MemoryManager::MemoryManager(Cartridge& cartridge)
		: m_Cartridge(cartridge)
	{
		RAM dummy{ 1 };

		m_RAMs[0xFF] = dummy;
	}

	MemoryManager::~MemoryManager()
	{

	}


	auto MemoryManager::AddChunk(MemoryChunk& chunk) -> void 
	{ 
		chunk.ID = static_cast<std::uint8_t>(m_Chunks.size()) + 1;
		m_Chunks.push_back(chunk); 

		if (chunk.Type == MemoryType::RAM || chunk.Type == MemoryType::IO)
			m_RAMs[chunk.ID] = RAM{ chunk.Size };
	}


	auto MemoryManager::ReadMemory(MemoryOwner owner, std::uint16_t address) -> std::uint8_t
	{
		for (auto& chunk : m_Chunks)
		{
			if (address >= chunk.StartAddress && address < chunk.StartAddress + chunk.Size && owner == chunk.Owner)
			{
				if (chunk.Type == MemoryType::ROM && owner == MemoryOwner::CPU)
					return m_Cartridge.GetROM(ROMType::Program).ReadAddress(address - chunk.StartAddress);
				else if (chunk.Type == MemoryType::ROM && owner == MemoryOwner::PPU)
					return m_Cartridge.GetROM(ROMType::Character).ReadAddress(address - chunk.StartAddress);

				if (chunk.Type == MemoryType::IO)
				{
					auto value = m_RAMs[chunk.ID].ReadAddress(address - chunk.StartAddress);

					// Reset Vblank and w-flag on PPU_STATUS read
					if (address == 0x2002)
					{
						m_RAMs[chunk.ID].WriteAddress(address - chunk.StartAddress, value & 0x7F);
						RegisterW = false;
					}

					if (address == 0x4016)
					{
						return ReadController(0);
					}

					if (address == 0x4017)
					{
						return ReadController(1);
					}

					return value;
				}
				else if (chunk.Type == MemoryType::RAM && owner == chunk.Owner)
					return m_RAMs[chunk.ID].ReadAddress(address - chunk.StartAddress);
			}
		}

		std::println("MemoryManager::ReadMemory - Unknown memory address {:04x}", address);

		return m_RAMs[0xFF].ReadAddress(0);
	}

	auto MemoryManager::WriteMemory(MemoryOwner owner, std::uint16_t address, std::uint8_t value, bool skipPPUCheck) -> void
	{
		// Special handling for PPUADDR and PPUDATA transfers
		if (!skipPPUCheck && (address >= 0x2003 && address <= 0x2007))
			HandlePPUAddress(address, value);

		for (auto& chunk : m_Chunks)
		{
			if (owner == chunk.Owner)
			{
				if (chunk.Type == MemoryType::RAM)
				{
					if (address >= chunk.StartAddress && address < chunk.StartAddress + chunk.Size && owner == chunk.Owner)
					{
						m_RAMs[chunk.ID].WriteAddress(address - chunk.StartAddress, value);
						return;
					}
				}
				else if (chunk.Type == MemoryType::IO)
				{
					if (address >= chunk.StartAddress && address < chunk.StartAddress + chunk.Size)
					{
						m_RAMs[chunk.ID].WriteAddress(address - chunk.StartAddress, value);
						return;
					}
				}
			}
		}

		std::println("MemoryManager::WriteMemory - Unknown memory address {:04x}", address);
	}

	auto MemoryManager::GetIOAddress(std::uint16_t address) -> std::uint8_t&
	{
		for (auto& chunk : m_Chunks)
		{
			if (chunk.Type == MemoryType::IO)
			{
				if (address >= chunk.StartAddress && address < chunk.StartAddress + chunk.Size)
					return m_RAMs[chunk.ID].GetAddress(address - chunk.StartAddress);
			}
		}

		// The zero here might be uninitialized.
		return m_RAMs[0xFF].GetAddress(0);
	}

	auto MemoryManager::DMATransfer(MemoryOwner targetOwner, std::uint8_t value) -> void
	{
		std::uint8_t addressHigh{};
		std::uint16_t length{};

		if (targetOwner == MemoryOwner::PPU)
		{
//			std::println(" DMA 0x4014: {:02x}", value);
			length = 0x100;
		}
		else if (targetOwner == MemoryOwner::ASU)
		{
			length = 2;
		}

		std::uint16_t address = (value << 8) & 0xFF00;

		for (auto i = 0; i < length; i++)
		{
			WriteMemory(targetOwner, i, ReadMemory(MemoryOwner::CPU, address + i));
		}
	}

	auto MemoryManager::HandlePPUAddress(std::uint16_t address, std::uint8_t value) -> void
	{
		switch (address)
		{
			case 0x2003:
			{
//				std::println(" OAMAddress: {:02}", value);
				OAMAddress = value & 0x00FF;
				break;
			}

			case 0x2004:
			{
				WriteMemory(MemoryOwner::PPU, OAMAddress, value, true);
				break;
			}

			case 0x2005:
			{
				if (!RegisterW)
					ScrollX = value;
				else
					ScrollY = value;

//				std::println(" ScrollX: {:02x}\n ScrollY: {:02x}", ScrollX, ScrollY);

				RegisterW = !RegisterW;
				break;
			}

			case 0x2006:
			{
				if (!RegisterW)
					PPUAddress = ((value & 0x3F) << 8) & 0xFF00;
				else
				{
					PPUAddress += value;	
					std::println("PPUAddress: {:04x}", PPUAddress);
				}

				RegisterW = !RegisterW;

				break;
			}

			case 0x2007:
			{
//				std::println("PPUData write: {:04x} = {:02x}", PPUAddress, value);
				WriteMemory(MemoryOwner::PPU, PPUAddress++, value, true);
				break;
			}
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


	auto MemoryManager::ViewMemory() -> void
	{
		ImGui::Begin("Memory");

		if (ImGui::InputInt("Chunk", &SelectedChunk))
		{
			if (SelectedChunk < 0) SelectedChunk = 0;
			if (SelectedChunk >= m_Chunks.size()) SelectedChunk = m_Chunks.size() - 1;
		}

		// Chunk info block
		{
			auto chunk = m_Chunks.at(SelectedChunk);

			if (chunk.Type == MemoryType::ROM) ImGui::Text("ChunkType: ROM");
			if (chunk.Type == MemoryType::RAM) ImGui::Text("ChunkType: RAM");
			if (chunk.Type == MemoryType::IO)  ImGui::Text("ChunkType: IO");

			ImGui::Text("Start address: %04x", chunk.StartAddress);
			ImGui::Text("Length: %04x", chunk.Size);

			if (chunk.Owner == MemoryOwner::CPU) ImGui::Text("Owner: CPU");
			if (chunk.Owner == MemoryOwner::PPU) ImGui::Text("Owner: PPU");
			if (chunk.Owner == MemoryOwner::ASU) ImGui::Text("Owner: ASU");

			ImGui::Separator();

			ImGui::Text("%s", chunk.Name.c_str());

			ImGui::Separator();

			if (ImGui::InputInt("Page", &MemoryPage))
			{
				if (MemoryPage < 0 || chunk.Size <= 0x100) MemoryPage = 0;
				if (MemoryPage > ((chunk.Size / 256) - 1) && chunk.Size > 0x100) MemoryPage = (chunk.Size / 256) - 1;
			}

			ImGui::Separator();

			if (chunk.Type == MemoryType::ROM && chunk.Owner == MemoryOwner::CPU) ViewPage(m_Cartridge.GetROM(ROMType::Program).GetData(), chunk.StartAddress);
			if (chunk.Type == MemoryType::ROM && chunk.Owner == MemoryOwner::PPU) ViewPage(m_Cartridge.GetROM(ROMType::Character).GetData(), chunk.StartAddress);

			if (chunk.Type == MemoryType::RAM || chunk.Type == MemoryType::IO)
			{
				ViewPage(m_RAMs.at(chunk.ID).GetData(), chunk.StartAddress);
			}
		}

		ImGui::End();
	}

}
