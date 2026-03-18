#include "emu/ppu/ppu.h"
#include "emu/cpu6502/cpu.h"
#include "emu/memory/memorymanager.h"
#include "emu/system/powerhandler.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <print>
#include <thread>
#include <ranges>
#include <unordered_map>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>


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

	static bool OddFrame = false;

	static std::uint32_t MasterClockFrequency = 21477272u;

	static std::vector<std::uint32_t> PaletteColors
	{
		0x333, 0x014, 0x006, 0x326, 0x403, 0x503, 0x510, 0x420, 0x320, 0x120, 0x031, 0x040, 0x022, 0x111, 0x003, 0x020,
		0x555, 0x036, 0x027, 0x407, 0x507, 0x704, 0x700, 0x630, 0x430, 0x140, 0x040, 0x053, 0x044, 0x222, 0x200, 0x310,
		0x777, 0x357, 0x447, 0x637, 0x707, 0x737, 0x740, 0x750, 0x660, 0x360, 0x070, 0x276, 0x077, 0x444, 0x000, 0x000,
		0x777, 0x567, 0x657, 0x757, 0x747, 0x755, 0x764, 0x770, 0x773, 0x572, 0x473, 0x276, 0x467, 0x666, 0x653, 0x760,
	};

	struct SpriteData
	{
		std::uint8_t YPosition;
		std::uint8_t TileIndex;
		std::uint8_t Attributes;
		std::uint8_t XPosition;
	};

	struct TileData
	{
		std::array<std::uint8_t, 64> PixelValues;
	};

	static std::vector<std::uint8_t> ImageData;
	static std::vector<std::uint16_t> NametableData;

	static std::unordered_map<std::uint16_t, TileData> Tilemap;
//	static std::unordered_map<std::uint8_t, TileData> Tilemap;
	static std::vector<SpriteData> Sprites;


	std::atomic<bool> SceneIsDrawing{ false };



	auto LoadTile(MemoryManager& memoryManager, std::uint16_t baseAddress, std::uint16_t tileID) -> void
	{
		TileData newTileData;

		std::vector<std::uint8_t> tileData(16);
		std::uint16_t tileByteAddress = 0;

		for (auto& tileByte : tileData)
		{
			tileByte = memoryManager.ReadCharROM(baseAddress + tileID * 16u + tileByteAddress++);
		}

		auto index{ 0u };

		for (auto row = 0; row < 8; row++)
		{
			for (auto col = 0; col < 8; col++)
			{
				auto pixelValue{ 0u };

				auto byte1 = tileData.at(row);
				auto byte2 = tileData.at(row + 8);

				if (byte1 & 1 << (7 - col)) pixelValue += 1;
				if (byte2 & 1 << (7 - col)) pixelValue += 2;

				newTileData.PixelValues[index++] = pixelValue;
			}
		}

		Tilemap[tileID] = newTileData;
	}

	auto LoadTiles(MemoryManager& memoryManager) -> void
	{
		for (auto index = 0; index < 512; index++)
		{
			LoadTile(memoryManager, 0x0000, index);
		}
	}


	PPU::PPU(PowerHandler& powerHandler, MemoryManager& memoryManager, std::uint8_t nametableAlignment)
		: m_PowerHandler(powerHandler), m_MemoryManager(memoryManager), m_NametableAlignment(nametableAlignment)
	{
		m_Pixels.resize(256 * 240);
		ImageData.resize(256u * 240u * 4u);

		std::uint16_t index{};

		LoadTiles(m_MemoryManager);
	}

	auto PPU::IsDrawing() -> bool { return SceneIsDrawing.load(); }

	auto PPU::GetImageData() -> std::vector<std::uint8_t>&
	{
		while (SceneIsDrawing.load())
			;

		return ImageData;
	}

	auto PPU::Execute() -> void
	{
		std::println("Starting PPU");

		m_Executing.store(true);

		auto framesPerSecond = 60.0988f;
		auto frameTime = 1.0f / framesPerSecond;


		const auto frequency = MasterClockFrequency / 4;
		const auto cycleTime = 1'000'000'000 / frequency;

		LARGE_INTEGER li{};
		if (!QueryPerformanceFrequency(&li))
		{
			std::println("QueryPerformanceFrequency failed");
		}

		auto freq = li.QuadPart; //  / 1'000.0;
		auto frequencyDivider = static_cast<double>(freq) / frequency;

		std::println("PPU frequency: {} Hz, PerformanceCounterFrequency = {} Hz, frequencyDivider = {}", frequency, freq, frequencyDivider);

		std::println("Frametime: {}s", frameTime);

		NametableData.resize(32 * 30 * 2);

		LARGE_INTEGER vblankCount{};
		LARGE_INTEGER vblankStartCount{};


		LARGE_INTEGER startCount{};
		LARGE_INTEGER endCount{};

//		QueryPerformanceCounter(&vblankStartCount);

		while (m_Executing.load())
		{
			OddFrame = !OddFrame;

			QueryPerformanceCounter(&startCount);

			auto startTime = std::chrono::steady_clock::now();

			if (m_PowerHandler.GetState() == PowerState::SingleStep || m_PowerHandler.GetState() == PowerState::Off)
				m_PowerHandler.SetState(PowerState::Suspended);
			{
				std::unique_lock<std::mutex> lock(m_Mutex);
				m_CV.wait(lock, [&] {
					return m_PowerHandler.GetState() != PowerState::Suspended || m_Executing.load() == 0;
					});
			}

			m_MemoryManager.ClearPPUIOBit(PPUSTATUS, 0x40);



//			NametableData.clear();

			// Set VBlank flag
			{
				m_MemoryManager.SetPPUIOBit(PPUSTATUS, 0x80);
			}

			QueryPerformanceCounter(&vblankStartCount);

			do 
			{
				QueryPerformanceCounter(&vblankCount);
			} while ((vblankCount.QuadPart - vblankStartCount.QuadPart) < (6820 * frequencyDivider));


			if (m_MemoryManager.ReadPPUIO(PPUCTRL) & 0x80 && (m_MemoryManager.GetPPUIOBit(PPUSTATUS) & 0x80))
			{
				//				std::println("TriggerNMI");
				CPU::TriggerNMI();
			}

			// Clear VBlank flag
			{
				m_MemoryManager.ClearPPUIOBit(PPUSTATUS, 0x80);
			}

			// Set Sprite0 hit flag
			{
//				m_MemoryManager.SetPPUIOBit(PPUSTATUS, 0x40);

			}

//			while (CPU::NMIRunning())
//				;

			// Generate image data

//			while (CPU::NMIRunning())
//				
//			std::this_thread::sleep_for(4ms);

//			if (OddFrame)
//			{
			SceneIsDrawing.store(true);
			GenerateImageData(ImageData);
			SceneIsDrawing.store(false);
//			}

//			std::this_thread::sleep_for(8ms);

			// Clear Sprite0 hit flag
			{
//				m_MemoryManager.ClearPPUIOBit(PPUSTATUS, 0x40);
			}

//			else
//				std::println("Skipping NMI");

//			std::this_thread::sleep_for(2ms);
//			std::this_thread::sleep_for(12ms);


			do
			{
				QueryPerformanceCounter(&endCount);
			} while ((endCount.QuadPart - startCount.QuadPart) < (240 * 341 * frequencyDivider)); // * frequencyDivider * 341 < (240 * 341 * frequencyDivider));

//			auto endTime = std::chrono::steady_clock::now();

//			std::println("Frametime: {}", std::chrono::duration<double>(endTime - startTime).count());
		}

		std::println("Stopping PPU");
	}

	auto PPU::Stop() -> void
	{
		m_Executing.store(false);
		m_CV.notify_all();
	}

	auto PPU::UpdatePowerState() -> void
	{
		m_CV.notify_all();
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
		return m_MemoryManager.ReadPPURAM(address);
	}

	auto PPU::WriteMemory(std::uint16_t address, std::uint8_t value) -> void
	{
		m_MemoryManager.WritePPURAM(address, value);
	}

	auto DrawSprite(SpriteData& spriteData, std::uint8_t spriteIndex, std::uint8_t backgroundIndex, MemoryManager& memoryManager) -> void
	{
		// Check sprite 0 collision
//		if (spriteIndex == 0)
//		{
//			auto& sprite = Sprites[0];
//			auto& tileData = Tilemap.at(sprite.TileIndex);
//			auto& backgroundTileData = Tilemap.at(backgroundIndex);
//	
//			for (auto i = 0; i < 64; i++)
//			{
//				if ((tileData.PixelValues.at(i) & backgroundTileData.PixelValues.at(i)) != 0)
//				{
//					auto value = memoryManager.ReadPPUIO(PPUSTATUS);
//					value |= 0x40;
//					memoryManager.WritePPUIO(PPUSTATUS, value);
//
//					break;
//				}
//			}
//		}

		if (spriteIndex == 0)
			memoryManager.SetPPUIOBit(PPUSTATUS, 0x40);

		for (auto yIndex = 0; yIndex < 8; yIndex++)
		{
			for (auto xIndex = 0; xIndex < 8; xIndex++)
			{
				if (Tilemap.at(spriteData.TileIndex).PixelValues.at(yIndex * 8 + xIndex) == 0)
					continue;

				std::uint8_t paletteIndex = 0x10 | ((spriteData.Attributes & 0x3) << 2) | (Tilemap.at(spriteData.TileIndex).PixelValues.at(yIndex * 8 + xIndex) & 0x3);
				auto paletteColor = memoryManager.ReadPPURAM(0x3F00 + paletteIndex);

				auto color = PaletteColors.at(paletteColor);

				auto posX = spriteData.XPosition + xIndex;
				auto posY = spriteData.YPosition + yIndex;

				if (spriteData.Attributes & 0x40)
					posX = spriteData.XPosition + 8 - xIndex;
				if (spriteData.Attributes & 0x80)
					posY = spriteData.YPosition + 8 - yIndex;

				if (posX < 0 || posX >= 256 || posY < 0 || posY >= 240)
					continue;

				ImageData.at((posY * 256 + posX) * 4 + 0) = ((color & 0x0F00) >> 8) / 7.0f * 255;
				ImageData.at((posY * 256 + posX) * 4 + 1) = ((color & 0x00F0) >> 4) / 7.0f * 255;
				ImageData.at((posY * 256 + posX) * 4 + 2) = (color & 0x000F) / 7.0f * 255;
				ImageData.at((posY * 256 + posX) * 4 + 3) = 255;
			}
		}


	}

	auto DrawTile(std::uint16_t tileID, std::uint8_t tileAttribute, std::uint16_t x, std::uint16_t y, std::uint8_t sizeY, std::uint8_t scrollX, MemoryManager& memoryManager) -> void
	{
		std::uint8_t spriteSelect{ 0 };

//		auto xOffset = memoryManager.GetXRegister();

		if (!Tilemap.contains(tileID))
		{
			std::println("Tile {:02x} unavailable", tileID);
			return;
		}

		for (auto yIndex = 0; yIndex < sizeY; yIndex++)
		{
			for (auto xIndex = 0; xIndex < 8; xIndex++)
			{
				std::uint8_t paletteIndex = (spriteSelect << 4) | ((tileAttribute & 0x3) << 2) | (Tilemap[tileID].PixelValues.at(yIndex * 8 + xIndex) & 0x3);
				auto paletteColor = memoryManager.ReadPPURAM(0x3F00 + paletteIndex);

				if (paletteColor == 0x0f && paletteIndex % 4 == 0)
					paletteColor = 0x22;

				auto color = PaletteColors.at(paletteColor);

				auto posX = x * 8 + xIndex - scrollX;
				auto posY = y * 8 + yIndex;

				if (posX < 0 || posX >= 256)
					continue;

				ImageData.at((posY * 256 + posX) * 4 + 0) = ((color & 0x0F00) >> 8) / 7.0f * 255;
				ImageData.at((posY * 256 + posX) * 4 + 1) = ((color & 0x00F0) >> 4) / 7.0f * 255;
				ImageData.at((posY * 256 + posX) * 4 + 2) = (color & 0x000F) / 7.0f * 255;
				ImageData.at((posY * 256 + posX) * 4 + 3) = 255;
			}
		}
	}

	auto PPU::GenerateImageData(std::span<std::uint8_t> imageData) -> void
	{
		auto ppuCtrl = m_MemoryManager.ReadPPUIO(PPUCTRL);
		auto ppuMask = m_MemoryManager.ReadPPUIO(PPUMASK);

		std::uint16_t patternBaseAddress = ppuCtrl & 0x10 ? 0x1000 : 0x0000;
		std::uint16_t spritePatternTable = ppuCtrl & 0x08 ? 0x1000 : 0x0000;
		std::uint8_t spriteSize = ppuCtrl & 0x20 ? 16 : 8;

		auto scrollX = m_MemoryManager.GetScrollXRegister() + ((ppuCtrl & 0x1) << 8);
		auto scrollY = m_MemoryManager.GetScrollYRegister() + ((ppuCtrl & 0x2) << 8);

//		auto scrollT = m_MemoryManager.GetTRegister();
//		auto scrollV = m_MemoryManager.GetVRegister();
//		auto scrollFine = m_MemoryManager.GetXRegister();

//		auto scrollX = scrollT & 0x1F;
//		auto scrollY = 0;

		// Read sprites

		Sprites.clear();

		for (auto index = 0; index < 64; index++)
		{
			SpriteData sprite;
			sprite.YPosition = m_MemoryManager.ReadOAMRAM(index * 4);
			sprite.TileIndex = m_MemoryManager.ReadOAMRAM(index * 4 + 1) + (spritePatternTable == 0x1000 ? 0x100 : 0x0);
			sprite.Attributes = m_MemoryManager.ReadOAMRAM(index * 4 + 2);
			sprite.XPosition = m_MemoryManager.ReadOAMRAM(index * 4 + 3);

			if (sprite.YPosition >= 0xEF || sprite.XPosition >= 0xF9)
				continue;

			Sprites.push_back(sprite);
		}

		if (Sprites.size() >= 8)
			m_MemoryManager.SetPPUIOBit(PPUSTATUS, 0x20);

//		Tilemap.clear();

		std::uint16_t nametableOffset{ 0 };

		std::vector<std::uint8_t> attributeData(0x80);

//		scrollX = 15;

		// Load tile data
		for (auto y = 0; y < 30; y++)
		{
			for (auto x = 0; x < 64; x++)
			{
//				nametableOffset = m_MemoryManager.GetTRegister() & 0x0C00;
				nametableOffset = 0x400 * (ppuCtrl & 0x3);

				if (x >= 32)
				{
					nametableOffset ^= 0x400;
				}

//				std::uint8_t tileID = m_MemoryManager.ReadPPURAM(0x2000 + nametableOffset + y * 32 + x % 32); //xpos);
				std::uint8_t tileID = m_MemoryManager.ReadPPURAM(0x2000 + nametableOffset + y * 32 + x % 32); //xpos);

				NametableData[y * 64 + x] = tileID + (patternBaseAddress == 0x1000 ? 0x100 : 0x0);

//				if (!Tilemap.contains(tileID))
//					LoadTile(m_MemoryManager, patternBaseAddress, tileID);
			}
		}

		for (auto y = 0; y < 8; y++)
		{
			for (auto x = 0; x < 16; x++)
			{
//				nametableOffset = m_MemoryManager.GetTRegister() & 0x0C00;
				nametableOffset = 0x400 * (ppuCtrl & 0x3);

				if (x >= 8)
				{
					nametableOffset ^= 0x400;
				}

				attributeData[y * 16 + x] = m_MemoryManager.ReadPPURAM(0x2000 + nametableOffset + 0x3c0 + y * 8 + x % 8); // xpos);
			}
		}

//		if (m_MemoryManager.GetXRegister() != 0)
//			std::println("Scroll T: {:04x}  - Scroll V: {:04x}  - Scroll X: {:02x}", scrollT, scrollV, m_MemoryManager.GetXRegister());
			std::println("ScrollX : {}", scrollX);

		auto softScrollX = scrollX % 8;
		auto softScrollY = scrollY % 8;

		scrollX >>= 3;
		scrollY >>= 3;

		// Parse screen (tilewise)
		for (auto y = 0u; y < 30u; y++)
		{
			for (auto x = 0u; x < 33u; x++)
			{
				// Draw background
				// Disabled for debugging purposes
				if (!(ppuMask & 0x08))
					continue;

				if (x == 32 && softScrollX == 0)
					continue;

				std::uint16_t tile = ((y + scrollY) * 64u) + (((x + scrollX) % 64));
				std::uint16_t attribute = ((y + scrollY) / 4 * 16u) + (((x + scrollX) % 64) / 4);

				std::uint8_t tileAttributeX = ((x + scrollX) % 4) > 1 ? 1 : 0;
				std::uint8_t tileAttributeY = ((y + scrollY) % 4) > 1 ? 2 : 0;
				
				// Shift down attributeValue to correct block

				auto attributeValue = attributeData[attribute];
				std::uint8_t tileAttribute = (attributeValue >> (2 * (tileAttributeX + tileAttributeY))) & 0x3;

				DrawTile(NametableData[tile], tileAttribute, x, y, spriteSize, softScrollX, m_MemoryManager);
			}
		}

		if (ppuMask & 0x10)
		{
			// Load sprite tiles
			for (auto sprite : Sprites)
			{
				DrawSprite(sprite, 0, 0, m_MemoryManager);
			}
		}
	}

}
