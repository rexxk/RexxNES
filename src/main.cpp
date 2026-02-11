#include "display/texture.h"
#include "emu/cartridge/cartridge.h"
#include "emu/cpu6502/cpu.h"
#include "emu/memory/dma.h"
#include "emu/memory/memory.h"
#include "emu/memory/memorymanager.h"
#include "emu/ppu/ppu.h"
#include "input/controller.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <cstdint>
#include <fstream>
#include <memory>
#include <print>
#include <thread>
#include <vector>


struct ROMHeader
{
	std::uint8_t Magic[4]{};
	std::uint8_t PrgRomSize{};
	std::uint8_t ChrRomSize{};
	std::uint8_t Flags1{};
	std::uint8_t Flags2{};
	std::uint8_t Flags3{};
	std::uint8_t Flags4{};
	std::uint8_t Flags5{};
	std::uint8_t Reserved[5]{};
};


auto main() -> int
{
	std::println("RexxNES 2026 - emulation at its worst");

	if (glfwInit() == GLFW_FALSE)
	{
		std::println("Failed to initialize GLFW");
		return -1;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
	glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(800, 600, "RexxNES", nullptr, nullptr);

	glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			switch (key)
			{
				case 32:
				{
					emu::Controller::SetState(emu::Button::Select, action == GLFW_PRESS);
					break;
				}

				case 257:
				{
					emu::Controller::SetState(emu::Button::Start, action == GLFW_PRESS);
					break;
				}

				case 262:
				{
					emu::Controller::SetState(emu::Button::Right, action == GLFW_PRESS);
					break;
				}

				case 263:
				{
					emu::Controller::SetState(emu::Button::Left, action == GLFW_PRESS);
					break;
				}

				case 264:
				{
					emu::Controller::SetState(emu::Button::Down, action == GLFW_PRESS);
					break;
				}

				case 265:
				{
					emu::Controller::SetState(emu::Button::Up, action == GLFW_PRESS);
					break;
				}

				case 341:
				{
					emu::Controller::SetState(emu::Button::A, action == GLFW_PRESS);
					break;
				}

				case 342:
				{
					emu::Controller::SetState(emu::Button::B, action == GLFW_PRESS);
					break;
				}

				default:
				{
					break;
				}

			}

//			std::println("Key {} ({}) released", key, scancode);

		});

	glfwMakeContextCurrent(window);

	gladLoadGL();

	std::println("Initializing ImGui");

	IMGUI_CHECKVERSION();

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();
	
	emu::MemoryManager memoryManager;

	emu::Cartridge cartridge("rom/SuperMarioBros.nes");

	{
		emu::MemoryChunk chunk{};
		chunk.StartAddress = 0x8000;
		chunk.Size = cartridge.GetProgramROM().GetSize();
		chunk.Owner = emu::MemoryOwner::CPU;
		chunk.Type = emu::MemoryType::ROM;

		memoryManager.AddChunk(chunk);
	}
	
	{
		emu::MemoryChunk chunk{};
		chunk.StartAddress = 0x0000;
		chunk.Size = cartridge.GetCharROM().GetSize();
		chunk.Owner = emu::MemoryOwner::PPU;
		chunk.Type = emu::MemoryType::ROM;

		memoryManager.AddChunk(chunk);
	}

	{
		emu::MemoryChunk chunk{};
		chunk.StartAddress = 0x2000;
		chunk.Size = 8;
		chunk.Owner = emu::MemoryOwner::PPU;
		chunk.Type = emu::MemoryType::IO;
	}

	{
		emu::MemoryChunk chunk{};
		chunk.StartAddress = 0x4000;
		chunk.Size = 0x18;
		chunk.Owner = emu::MemoryOwner::PPU;
		chunk.Type = emu::MemoryType::IO;
	}

	std::ifstream fs("rom/SuperMarioBros.nes", std::ios::in | std::ios::binary);

	if (!fs.is_open())
	{
		std::println("Failed to open rom file");
		return -1;
	}

	fs.seekg(0, fs.end);
	std::size_t romSize = fs.tellg();
	fs.seekg(0, fs.beg);

//	std::println("ROM size: {} bytes", romSize);

	ROMHeader romHeader{};
	fs.read((char*)&romHeader, sizeof(ROMHeader));

	if (romHeader.Flags1 & 0x04)
	{
		std::vector<uint8_t> trainerData(512);
		std::println("512 bytes trainer is present");
		fs.read((char*)trainerData.data(), 512);
	}

//	std::println("Reading prog ROM");

	std::vector<uint8_t> programRom(romHeader.PrgRomSize * 0x4000);
	fs.read((char*)programRom.data(), romHeader.PrgRomSize * 0x4000);

//	std::println("Reading char ROM");

	std::vector<uint8_t> charRom(romHeader.ChrRomSize * 0x2000);
	fs.read((char*)charRom.data(), romHeader.ChrRomSize * 0x2000);

//	std::println("File location: {}", static_cast<std::size_t>(fs.tellg()));

	//std::uint8_t Flags1{};
	//std::uint8_t Flags2{};
	//std::uint8_t Flags3{};
	//std::uint8_t Flags4{};
	//std::uint8_t Flags5{};
//	std::println(" * Program ROM size : {} bytes", romHeader.PrgRomSize * 0x4000);
//	std::println(" * Char ROM size    : {} bytes", romHeader.ChrRomSize * 0x2000);
//
//	std::println(" * Flags 6          : {:08b}", romHeader.Flags1);
//	std::println(" * Flags 7          : {:08b}", romHeader.Flags2);
//	std::println(" * Flags 8          : {:08b}", romHeader.Flags3);
//	std::println(" * Flags 9          : {:08b}", romHeader.Flags4);
//	std::println(" * Flags 10         : {:08b}", romHeader.Flags5);

	fs.close();

	std::uint8_t mapperID = (romHeader.Flags2 & 0xF0) + ((romHeader.Flags1 & 0xF0) >> 4);

	std::println("Mapper ID#: {}", mapperID);

	emu::Memory cpuMemory{64};
	cpuMemory.InstallROM(0x8000, programRom);

	emu::Memory ppuMemory{32};
	ppuMemory.InstallROM(0x0000, charRom);

	std::uint8_t nametableAlignment = romHeader.Flags1 & 0x01;
	emu::PPU ppu{ ppuMemory, cpuMemory, nametableAlignment };

	std::array<std::uint8_t, 0x100> asuMemory;
	emu::DMA oamDMA{ 0x4014, cpuMemory, ppu.GetInternalMemory() };
	emu::DMA dmcDMA{ 0x4015, cpuMemory, asuMemory };

	emu::CPU cpu{ cpuMemory, oamDMA, dmcDMA };

	std::thread cpuThread(&emu::CPU::Execute, &cpu, 0);
	std::thread ppuThread(&emu::PPU::Execute, &ppu);

	std::vector<std::uint8_t> imageData;
	imageData.resize(256u * 240u * 4u);

	int memoryPage{ 0u };
	int ppuMemoryPage{ 32u };

	emu::Texture displayTexture{ 256u, 240u };


	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// ImGui controls
		{
			ImGui::Begin("CPU status");

//			ImGui::Text("Frame count: %d", frameCount++);

			auto flags = cpu.GetFlags();
			ImGui::Text("Flag register:");

			char flagString[9];
			(flags & 0x80) ? flagString[0] = 'N' : flagString[0] = 'n';
			(flags & 0x40) ? flagString[1] = 'V' : flagString[1] = 'v';
			(flags & 0x20) ? flagString[2] = '-' : flagString[2] = '-';
			(flags & 0x10) ? flagString[3] = 'B' : flagString[3] = 'b';
			(flags & 0x08) ? flagString[4] = 'D' : flagString[4] = 'd';
			(flags & 0x04) ? flagString[5] = 'I' : flagString[5] = 'i';
			(flags & 0x02) ? flagString[6] = 'Z' : flagString[6] = 'z';
			(flags & 0x01) ? flagString[7] = 'C' : flagString[7] = 'c';
			flagString[8] = '\0';

			ImGui::Text("%s", flagString);

			auto registers = cpu.GetRegisters();

			ImGui::Text("\nRegisters:");
			ImGui::Text("  A : %02x", registers.A);
			ImGui::Text("  X : %02x", registers.X);
			ImGui::Text("  Y : %02x", registers.Y);
			ImGui::Text(" SP : %02x", registers.SP);
			ImGui::Text(" PC : %04x", registers.PC);

			ImGui::Separator();

			ImGui::Text("Execution control");

			if (ImGui::Button("Run")) cpu.SetRunningMode(emu::RunningMode::Run);
			ImGui::SameLine();
			if (ImGui::Button("Halt")) cpu.SetRunningMode(emu::RunningMode::Halt);
			ImGui::SameLine();
			if (ImGui::Button("Step")) cpu.SetRunningMode(emu::RunningMode::Step);

			ImGui::Separator();

			if (ImGui::Button("Trigger NMI")) cpu.TriggerNMI();

			ImGui::End();
		}

		{
			ImGui::Begin("Memory");

			ImGui::InputInt("Page", &memoryPage);
			if (memoryPage < 0) memoryPage = 0;
			if (memoryPage > 0xFF) memoryPage = 0xFF;

			ImGui::Separator();

			cpuMemory.ViewPage(memoryPage);

			ImGui::End();
		}

		{
			ImGui::Begin("PPU memory");

			ImGui::InputInt("Page", &ppuMemoryPage);
			if (ppuMemoryPage < 0) ppuMemoryPage = 0;
			if (ppuMemoryPage > 0x7F) ppuMemoryPage = 0x7F;

			ImGui::Separator();

			ppuMemory.ViewPage(ppuMemoryPage);

			ImGui::End();
		}

		{
			ImGui::Begin("Graphics");

			ppu.GenerateImageData(imageData);
			displayTexture.SetData(imageData);
//			UpdateTexture(displayTexture);
			ImGui::Image(displayTexture.GetTexture(), ImVec2{ 512, 480 });

			ImGui::End();
		}

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);

	}

	cpu.Stop();
	ppu.Stop();

	ppuThread.join();
	cpuThread.join();
//	cpu.Execute(program, 0x1000);

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();

	return 0;
}
