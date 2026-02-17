#include "emu/memory/memorymanager.h"

#include <print>

#include "imgui.h"



namespace emu
{


	static int SelectedChunk{ 0 };
	static int MemoryPage{ 0 };


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
					return m_RAMs[chunk.ID].ReadAddress(address - chunk.StartAddress);
				else if (chunk.Type == MemoryType::RAM && owner == chunk.Owner)
					return m_RAMs[chunk.ID].ReadAddress(address - chunk.StartAddress);
			}
		}

		std::println("MemoryManager::ReadMemory - Unknown memory address {:04x}", address);

		return m_RAMs[0xFF].ReadAddress(0);
	}

	auto MemoryManager::WriteMemory(MemoryOwner owner, std::uint16_t address, std::uint8_t value) -> void
	{
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
					return m_RAMs[chunk.ID].GetAddress(address);
			}
		}

		// The zero here might be uninitialized.
		return m_RAMs[0xFF].GetAddress(0);
	}

	auto MemoryManager::DMATransfer(MemoryOwner targetOwner) -> void
	{
		std::uint8_t addressHigh{};
		std::uint16_t length{};

		if (targetOwner == MemoryOwner::PPU)
		{
			addressHigh = ReadMemory(MemoryOwner::CPU, 0x4014);
			length = 0x100;
		}
		else if (targetOwner == MemoryOwner::ASU)
		{
			addressHigh = ReadMemory(MemoryOwner::CPU, 0x4015);
			length = 2;
		}

		std::uint16_t address = (addressHigh << 8) & 0xFF00;

		for (auto i = 0; i < length; i++)
		{
			WriteMemory(targetOwner, i, ReadMemory(MemoryOwner::CPU, address + i));
		}
	}


	auto ViewPage(std::span<std::uint8_t> memory, std::uint16_t address)
	{
		auto startAddress = MemoryPage * 0x200;

		for (auto row = 0; row < 16; row++)
		{
			std::string str = std::format("{:04x} : ", row * 16 + startAddress + address);

			for (auto col = 0; col < 16; col++)
			{
				str += std::format("{:02x} ", memory[row * 16 + startAddress + col]);
			}

			ImGui::Text("%s", str.c_str());
		}
	}


	auto MemoryManager::ViewMemory() -> void
	{
		ImGui::Begin("Memory");

		ImGui::InputInt("Chunk", &SelectedChunk);
		if (SelectedChunk < 0) SelectedChunk = 0;
		if (SelectedChunk >= m_Chunks.size()) SelectedChunk = m_Chunks.size() - 1;

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

			ImGui::InputInt("Page", &MemoryPage);
			if (MemoryPage < 0 || chunk.Size <= 0x200) MemoryPage = 0;
			if (MemoryPage > ((chunk.Size / 512) - 1) && chunk.Size > 0x200) MemoryPage = (chunk.Size / 512) - 1;

			ImGui::Separator();

			if (chunk.Type == MemoryType::ROM && chunk.Owner == MemoryOwner::CPU) ViewPage(m_Cartridge.GetROM(ROMType::Program).GetData(), chunk.StartAddress);
			if (chunk.Type == MemoryType::ROM && chunk.Owner == MemoryOwner::PPU) ViewPage(m_Cartridge.GetROM(ROMType::Character).GetData(), chunk.StartAddress);

			if (chunk.Type == MemoryType::RAM)
			{
				ViewPage(m_RAMs.at(chunk.ID).GetData(), chunk.StartAddress);
			}
		}


//		cpuMemory.ViewPage(memoryPage);

		ImGui::End();
	}

}
