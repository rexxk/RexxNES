#pragma once

#include "emu/memory/memorymanager.h"
#include "emu/system/powerhandler.h"

#include <atomic>
#include <condition_variable>
#include <mutex>


namespace emu
{

	class APU
	{
	public:
		APU() = delete;
		explicit APU(PowerHandler& powerHandler, MemoryManager& memoryManager);

		auto Stop() -> void;
		auto UpdatePowerState() -> void;
		auto Execute() -> void;

	private:
		PowerHandler& m_PowerHandler;
		MemoryManager& m_MemoryManager;

		std::atomic<bool> m_Executing{ false };

		std::condition_variable m_CV{};
		std::mutex m_Mutex{};
	};


}
