#include "emu/ppu/ppu.h"
#include "emu/cpu6502/cpu.h"
#include "emu/memory/memory.h"

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
	
	static std::uint16_t RegV{};
	static std::uint16_t RegT{};
	static std::uint8_t RegX{};
	static std::uint8_t RegW{};
	
	
	static Memory PPU_CIRAM{ 8 };
	static std::uint16_t PPUAddress{};



	PPU::PPU(Memory& ppuMemory, Memory& cpuMemory, std::uint8_t nametableAlignment)
		: m_PPUMemory(ppuMemory), m_CPUMemory(cpuMemory), m_NametableAlignment(nametableAlignment)
	{
		m_Pixels.resize(256 * 240);
	}


	auto PPU::WritePPUAddress(std::uint8_t value) -> void
	{
		if (RegW == 0) PPUAddress = (value << 8) & 0xFF00;
		else if (RegW == 1) PPUAddress |= value;
	}

	auto PPU::WritePPUData(std::uint8_t value, std::uint8_t increment) -> void
	{
		PPU_CIRAM.Write(PPUAddress - 0x2000, value);

		PPUAddress += increment;
	}

	auto PPU::ReadPPUAddress() -> std::uint16_t
	{
		return PPUAddress;
	}

	auto PPU::ReadPPUData(std::uint8_t increment) -> std::uint8_t
	{
		auto value = PPU_CIRAM.Read(PPUAddress - 0x2000);

		PPUAddress += increment;

		return value;
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
				std::memcpy(m_PPUMemory.GetData() + 0x2000, PPU_CIRAM.GetData(), 0x2000);
			}

			// Clear VBlank flag
			{
				auto value = m_CPUMemory.Read(PPUSTATUS);
				value &= 0x7F;
				m_CPUMemory.Write(PPUSTATUS, value);
			}

			// Do all frame processing
			{
				auto oamAddress = m_CPUMemory.Read(OAMADDR);
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
				auto value = m_CPUMemory.Read(PPUSTATUS);
				value |= 0x80;
				m_CPUMemory.Write(PPUSTATUS, value);
			}

			auto ppuCtrl = m_CPUMemory.Read(PPUCTRL);

			if (ppuCtrl & 0x80)
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
		return m_PPUMemory.Read(address);
	}

	auto PPU::WriteMemory(std::uint16_t address, std::uint8_t value) -> void
	{
		m_PPUMemory.Write(address, value);
	}

	auto PPU::GenerateImageData(std::span<std::uint8_t> imageData) -> void
	{
//		std::uint16_t baseAddress = 0x2000;
		auto ppuCtrl = m_CPUMemory.Read(0x2000);

		auto nametableOffset = ppuCtrl & 0x03;
		std::uint16_t patternBaseAddress = ppuCtrl & 0x10 ? 0x1000 : 0x0000;

		for (std::uint32_t y = 0u; y < 240u; y++)
		{
			for (std::uint32_t x = 0u; x < 256u; x += 8u)
			{
				std::uint16_t tile = (y / 8u) * (32u) + x / 8u;

				std::uint16_t attribute = (y / 16u) * 16u + x / 16u;

				auto attributeValue = PPU_CIRAM.Read(attribute + nametableOffset * 0x400 + 0x3c0);
				auto tileValue = PPU_CIRAM.Read(tile + nametableOffset * 0x400);

				std::vector<std::uint8_t> tileData(16);

				std::uint16_t tileByteAddress = 0;

				// TODO: Remove, used for hardcoded debug tests
				patternBaseAddress = 0;

				for (auto& tileByte : tileData)
					tileByte = m_PPUMemory.Read(patternBaseAddress + tileValue * 16u + tileByteAddress++);

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

					std::uint8_t colorValue = m_PPUMemory.Read(0x3F00 + paletteIndex);

					if (colorValue)
					{
						std::uint32_t position = { (y * 256u + x + col) * 4u };
						imageData[position + 0] = 255 / colorValue;
						imageData[position + 1] = 255 / colorValue;
						imageData[position + 2] = 255 / colorValue;
						imageData[position + 3] = 255;
					}

//					std::uint32_t position = { (y * 256u + x + i) * 4u };
//					imageData[position + 0] = tileData[0];
//					imageData[position + 1] = tileData[4];
//					imageData[position + 2] = tileData[8];
//					imageData[position + 3] = 255;
				}

			}
		}

	}

}
