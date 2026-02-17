#include "emu/ppu/ppu.h"
#include "emu/cpu6502/cpu.h"
#include "emu/memory/memorymanager.h"

#include <chrono>
#include <cstdint>
#include <print>
#include <thread>
#include <ranges>


using namespace std::chrono_literals;

namespace emu
{

	static constexpr std::uint16_t PPUCTRL = 0x2000;
	static constexpr std::uint16_t PPUMASK = 0x2001;
	static constexpr std::uint16_t PPUSTATUS = 0x2002;
	static constexpr std::uint16_t OAMADDR = 0x2003;
	static constexpr std::uint16_t OAMDATA = 0x2004;
	static constexpr std::uint16_t PPUSCROLL = 0x2005;
	static constexpr std::uint16_t PPUADDR = 0x2006;
	static constexpr std::uint16_t PPUDATA = 0x2007;
	static constexpr std::uint16_t OAMDMA = 0x4014;
	
	static constexpr std::uint16_t IO_PPUCTRL = 0x0;
	static constexpr std::uint16_t IO_PPUMASK = 0x1;
	static constexpr std::uint16_t IO_PPUSTATUS = 0x2;
	static constexpr std::uint16_t IO_OAMADDR = 0x3;
	static constexpr std::uint16_t IO_OAMDATA = 0x4;
	static constexpr std::uint16_t IO_PPUSCROLL = 0x5;
	static constexpr std::uint16_t IO_PPUADDR = 0x6;
	static constexpr std::uint16_t IO_PPUDATA = 0x7;


	static std::uint16_t RegV{};
	static std::uint16_t RegT{};
	static std::uint8_t RegX{};
	static std::uint8_t RegW{};
	
	static std::uint16_t PPUAddress{};

	static std::vector<std::uint32_t> PaletteColors
	{
		0x333, 0x014, 0x006, 0x326, 0x403, 0x503, 0x510, 0x420, 0x320, 0x120, 0x031, 0x040, 0x022, 0x111, 0x003, 0x020,
		0x555, 0x036, 0x027, 0x407, 0x507, 0x704, 0x700, 0x630, 0x430, 0x140, 0x040, 0x053, 0x044, 0x222, 0x200, 0x310,
		0x777, 0x357, 0x447, 0x637, 0x707, 0x737, 0x740, 0x750, 0x660, 0x360, 0x070, 0x276, 0x077, 0x444, 0x000, 0x000,
		0x777, 0x567, 0x657, 0x757, 0x747, 0x755, 0x764, 0x770, 0x773, 0x572, 0x473, 0x276, 0x467, 0x666, 0x653, 0x760,
	};

	static std::vector<std::uint8_t> Palette4 {
		0x18, 0x03, 0x1C, 0x28, 0x2E, 0x35, 0x01, 0x17, 0x10, 0x1F, 0x2A, 0x0E, 0x36, 0x37, 0x0B, 0x39,
		0x25, 0x1E, 0x12, 0x34, 0x2E, 0x1D, 0x06, 0x26, 0x3E, 0x1B, 0x22, 0x19, 0x04, 0x2E, 0x3A, 0x21,
		0x05, 0x0A, 0x07, 0x02, 0x13, 0x14, 0x00, 0x15, 0x0C, 0x3D, 0x11, 0x0F, 0x0D, 0x38, 0x2D, 0x24,
		0x33, 0x20, 0x08, 0x16, 0x3F, 0x2B, 0x20, 0x3C, 0x2E, 0x27, 0x23, 0x31, 0x29, 0x32, 0x2C, 0x09,
	};

	PPU::PPU(MemoryManager& memoryManager, std::uint8_t nametableAlignment)
		: m_MemoryManager(memoryManager), m_NametableAlignment(nametableAlignment)
	{
		m_Pixels.resize(256 * 240);

		std::uint16_t index{};

		{
			// OAM (256 bytes)
			MemoryChunk chunk{};
			chunk.StartAddress = 0x0000;
			chunk.Size = 0x200;
			chunk.Type = MemoryType::RAM;
			chunk.Owner = MemoryOwner::PPU;

			m_MemoryManager.AddChunk(chunk);
		}

		{
			// PPU RAM (2k bytes)
			MemoryChunk chunk{};
			// TODO: StartAddress can be rewired with custom cartridge hardware
			chunk.StartAddress = 0x2000;
			chunk.Size = 0x2000;
			chunk.Type = MemoryType::RAM;
			chunk.Owner = MemoryOwner::PPU;

			m_MemoryManager.AddChunk(chunk);
		}

		{
			// Palette
			MemoryChunk chunk{};
			chunk.StartAddress = 0x3F00;
			chunk.Size = 0x100;
			chunk.Type = MemoryType::RAM;
			chunk.Owner = MemoryOwner::PPU;

			m_MemoryManager.AddChunk(chunk);
		}

		for (auto& color : Palette4)
			m_MemoryManager.WriteMemory(MemoryOwner::PPU, 0x3F00 + index, Palette4.at(index++));
	}


	auto PPU::WritePPUAddress(std::uint8_t value) -> void
	{
		if (RegW == 0) PPUAddress = (value << 8) & 0xFF00;
		else if (RegW == 1) PPUAddress |= value;
	}

	auto PPU::WritePPUData(std::uint8_t value, std::uint8_t increment) -> void
	{
//		PPU_CIRAM.Write(PPUAddress - 0x2000, value);

		PPUAddress += increment;
	}

	auto PPU::ReadPPUAddress() -> std::uint16_t
	{
		return PPUAddress;
	}

	auto PPU::ReadPPUData(std::uint8_t increment) -> std::uint8_t
	{
//		auto value = PPU_CIRAM.Read(PPUAddress - 0x2000);

		PPUAddress += increment;

//		return value;
		return 0;
	}


	auto PPU::Execute() -> void
	{
		std::println("Starting PPU");

		m_Executing.store(true);

		auto framesPerSecond = 60.0988f;
		auto frameTime = 1.0f / framesPerSecond;

		std::println("Frametime: {}s", frameTime);

		while (m_Executing.load())
		{
			// Copy CIRAM to 0x2000 in PPU memory
			{
//				std::memcpy(m_PPUMemory.GetData() + 0x2000, PPU_CIRAM.GetData(), 0x2000);
			}

			// Clear VBlank flag
			{
//				m_MMIO[IO_PPUSTATUS] &= 0x7F;

//				auto value = m_MemoryManager.ReadMemory(MemoryOwner::PPU, IO_PPUSTATUS);
				auto& value = m_MemoryManager.GetIOAddress(PPUSTATUS);
				value &= 0x7F;
//				m_MemoryManager.WriteMemory(MemoryOwner::PPU, IO_PPUSTATUS, value);
			}

			// Do all frame processing
			{
//				auto oamAddress = m_MMIO[IO_OAMADDR];
			}


//			for (std::uint16_t scanline = 0; scanline < 241; scanline++)
//			{
//				ProcessScanline(scanline);
//			}

//			for (std::uint16_t index = 0; index < 32 * 30; index++)
//			{
//				auto nametableValue = ReadMemory(0x2000 + index);
//			}
//
//			for (std::uint16_t index = 0; index < 64; index++)
//			{
//				auto nametableAttribute = ReadMemory(0x23c0 + index);
//			}



			// Set VBlank flag
			{
//				auto value = m_MemoryManager.ReadMemory(MemoryOwner::PPU, IO_PPUSTATUS);
				auto& value = m_MemoryManager.GetIOAddress(PPUSTATUS);
				value |= 0x80;
//				m_MemoryManager.WriteMemory(MemoryOwner::PPU, IO_PPUSTATUS, value);
//				m_MMIO[IO_PPUSTATUS] |= 0x80;
			}

//			auto ppuCtrl = m_CPUMemory.Read(PPUCTRL);

//			if (ppuCtrl & 0x80)
//			if (m_MMIO[IO_PPUCTRL] & 0x80)
			if (auto& value = m_MemoryManager.GetIOAddress(PPUCTRL); value & 0x80)
				CPU::TriggerNMI();

			std::this_thread::sleep_for(16ms);

		}

		std::println("Stopping PPU");
	}

	auto PPU::Stop() -> void
	{
		m_Executing.store(false);
	}


	auto PPU::ProcessScanline(std::uint16_t scanline) -> std::uint16_t
	{
		std::uint16_t cycles{};

		const std::uint16_t nametableBase = 0x2000;

		// 2 bits per pixel - 4 pixels per byte
		// First "cycle" - fetch data
		// Second "cycle" - create pixels

		auto nametableValue = ReadMemory(0x2000);

		// Tile size - 8x8

		// Idea - read all tiles (32x30) and interpolate all 8 pixels per tile and scanline. Make the pixel data from that.

		return cycles;
	}


	auto PPU::ToggleW() -> void
	{
		++RegW &= 0x01;

	}
	
	auto PPU::ResetW() -> void
	{
		RegW = 0;
	}

	auto PPU::ReadMemory(std::uint16_t address) -> std::uint8_t
	{
		return m_MemoryManager.ReadMemory(MemoryOwner::PPU, address);
	}

	auto PPU::WriteMemory(std::uint16_t address, std::uint8_t value) -> void
	{
		m_MemoryManager.WriteMemory(MemoryOwner::PPU, address, value);
	}

	auto PPU::GenerateImageData(std::span<std::uint8_t> imageData) -> void
	{
//		auto nametableOffset = m_MMIO[IO_PPUCTRL] & 0x03;
		auto nametableOffset = m_MemoryManager.GetIOAddress(IO_PPUCTRL) & 0x03;
//		std::uint16_t patternBaseAddress = m_MMIO[IO_PPUCTRL] & 0x10 ? 0x1000 : 0x0000;
		std::uint16_t patternBaseAddress = m_MemoryManager.GetIOAddress(IO_PPUCTRL) & 0x10 ? 0x1000 : 0x0000;

		// Draw background
		for (std::uint32_t y = 0u; y < 240u; y++)
		{
			for (std::uint32_t x = 0u; x < 256u; x += 8u)
			{
				std::uint16_t tile = (y / 8u) * (32u) + x / 8u;

				std::uint16_t attribute = (y / 16u) * 16u + x / 16u;

//				auto attributeValue = PPU_CIRAM.Read(attribute + nametableOffset * 0x400 + 0x3c0);
				auto attributeValue = m_MemoryManager.ReadMemory(MemoryOwner::PPU, attribute + nametableOffset * 0x400 + 0x3c0);
//				auto tileValue = PPU_CIRAM.Read(tile + nametableOffset * 0x400);
				auto tileValue = m_MemoryManager.ReadMemory(MemoryOwner::PPU, tile + nametableOffset * 0x400);

				std::vector<std::uint8_t> tileData(16);

				std::uint16_t tileByteAddress = 0;

				// TODO: Remove, used for hardcoded debug tests
//				patternBaseAddress = 0;

				for (auto& tileByte : tileData)
					tileByte = m_MemoryManager.ReadMemory(MemoryOwner::PPU, patternBaseAddress + tileValue * 16u + tileByteAddress++);

				for (auto col = 0u; col < 8u; col++)
				{
					auto row = y % 8u;

					auto byte1 = tileData.at(col);
					auto byte2 = tileData.at(col + 8);

					std::uint8_t pixelValue{ 0u };

					byte1 & 1 << (7 - col) ? pixelValue += 1 : pixelValue;
					byte2 & 1 << (7 - col) ? pixelValue += 2 : pixelValue;

					// Get palette data from pixelValue (0-3)
					std::uint8_t spriteSelect = 0;
					std::uint8_t paletteIndex = spriteSelect << 4 + (attributeValue & 0x3) << 2 + pixelValue & 0x3;

//					std::uint8_t colorValue = m_PPUMemory.Read(0x3F00 + paletteIndex);

					auto color = PaletteColors.at(Palette4.at(paletteIndex));

					if (color)
					{
						std::uint32_t position = { (y * 256u + x + col) * 4u };
						imageData[position + 0] = ((color & 0x0F00) >> 8) / 7.0f * 255;
						imageData[position + 1] = ((color & 0x00F0) >> 4) / 7.0f * 255;
						imageData[position + 2] = (color & 0x000F) / 7.0f * 255;
						imageData[position + 3] = 255;
					}

				}

			}
		}

		// Draw sprites
		{

		}
	}

}
