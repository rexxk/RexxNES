#include "display/texture.h"
#include "emu/apu/apu.h"
#include "emu/cartridge/cartridge.h"
#include "emu/cpu6502/cpu.h"
#include "emu/memory/memorymanager.h"
#include "emu/ppu/ppu.h"
#include "emu/system/powerhandler.h"
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
					emu::Controller::SetState(emu::Button::Select, action == GLFW_PRESS || action == GLFW_REPEAT);
					break;
				}

				case 257:
				{
					emu::Controller::SetState(emu::Button::Start, action == GLFW_PRESS || action == GLFW_REPEAT);
					break;
				}

				case 262:
				{
					emu::Controller::SetState(emu::Button::Right, action == GLFW_PRESS || action == GLFW_REPEAT);
					break;
				}

				case 263:
				{
					emu::Controller::SetState(emu::Button::Left, action == GLFW_PRESS || action == GLFW_REPEAT);
					break;
				}

				case 264:
				{
					emu::Controller::SetState(emu::Button::Down, action == GLFW_PRESS || action == GLFW_REPEAT);
					break;
				}

				case 265:
				{
					emu::Controller::SetState(emu::Button::Up, action == GLFW_PRESS || action == GLFW_REPEAT);
					break;
				}

				case 341:
				{
					emu::Controller::SetState(emu::Button::A, action == GLFW_PRESS || action == GLFW_REPEAT);
					break;
				}

				case 342:
				{
					emu::Controller::SetState(emu::Button::B, action == GLFW_PRESS || action == GLFW_REPEAT);
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
	
	emu::Cartridge cartridge("rom/SuperMarioBros.nes");
//	emu::Cartridge cartridge("rom/DonkeyKong.nes");
	emu::MemoryManager memoryManager(cartridge);

	{
		emu::MemoryChunk chunk{};
		chunk.StartAddress = 0x8000;
		chunk.Size = cartridge.GetROM(emu::ROMType::Program).GetSize();
		chunk.Owner = emu::MemoryOwner::CPU;
		chunk.Type = emu::MemoryType::ROM;
		chunk.Name = "Program ROM";

		memoryManager.AddChunk(chunk);
	}
	
	{
		emu::MemoryChunk chunk{};
		chunk.StartAddress = 0x0000;
		chunk.Size = cartridge.GetROM(emu::ROMType::Character).GetSize();
		chunk.Owner = emu::MemoryOwner::PPU;
		chunk.Type = emu::MemoryType::ROM;
		chunk.Name = "Char ROM";

		memoryManager.AddChunk(chunk);
	}

	{
		emu::MemoryChunk chunk{};
		chunk.StartAddress = 0x2000;
		chunk.Size = 8;
		chunk.Owner = emu::MemoryOwner::CPU;
		chunk.Type = emu::MemoryType::IO;
		chunk.Name = "PPU IO";

		memoryManager.AddChunk(chunk);
	}

	{
		emu::MemoryChunk chunk{};
		chunk.StartAddress = 0x4000;
		chunk.Size = 0x18;
		chunk.Owner = emu::MemoryOwner::CPU;
		chunk.Type = emu::MemoryType::IO;
		chunk.Name = "CPU IO";

		memoryManager.AddChunk(chunk);
	}

	emu::PowerHandler powerHandler{ emu::PowerState::Off };

	emu::PPU ppu{ powerHandler, memoryManager, cartridge.GetAttributes().NametableMirroring };
	emu::APU apu{ powerHandler, memoryManager };
	emu::CPU cpu{ powerHandler, memoryManager };

	std::thread cpuThread(&emu::CPU::Execute, &cpu, 0);
	std::thread ppuThread(&emu::PPU::Execute, &ppu);
	std::thread apuThread(&emu::APU::Execute, &apu);


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

			//			if (ImGui::Button("Run")) cpu.SetRunningMode(emu::RunningMode::Run);
			//			ImGui::SameLine();
			//			if (ImGui::Button("Halt")) cpu.SetRunningMode(emu::RunningMode::Halt);
			//			ImGui::SameLine();
			//			if (ImGui::Button("Step")) cpu.SetRunningMode(emu::RunningMode::Step);

			if (ImGui::Button("Run"))
			{
				powerHandler.SetState(emu::PowerState::Run);
				cpu.UpdatePowerState();
				ppu.UpdatePowerState();
			}

			ImGui::SameLine();

			if (ImGui::Button("Halt"))
			{
				powerHandler.SetState(emu::PowerState::Suspended);
				cpu.UpdatePowerState();
				ppu.UpdatePowerState();
			}

			ImGui::SameLine();

			if (ImGui::Button("Step"))
			{
				powerHandler.SetState(emu::PowerState::SingleStep);
				cpu.UpdatePowerState();
				ppu.UpdatePowerState();
			}

			ImGui::Separator();

			if (ImGui::Button("Step to RTS"))
			{
				cpu.StepToRTS();

				powerHandler.SetState(emu::PowerState::Run);
				cpu.UpdatePowerState();
				ppu.UpdatePowerState();
			}

			ImGui::SameLine();
			if (ImGui::Button("Trigger NMI")) cpu.TriggerNMI();

			ImGui::End();
		}

		memoryManager.ViewMemory();

		{
			ImGui::Begin("Graphics");

			//			ppu.GenerateImageData(imageData);
			displayTexture.SetData(ppu.GetImageData());
			ImGui::Image(displayTexture.GetTexture(), ImVec2{ 512, 480 });

			ImGui::End();
		}

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);

	}

	cpu.Stop();
	ppu.Stop();
	apu.Stop();

	apuThread.join();
	ppuThread.join();
	cpuThread.join();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();

	return 0;
}
