#include "emu/apu/apu.h"



namespace emu
{

	APU::APU(PowerHandler& powerHandler, MemoryManager& memoryManager)
		: m_PowerHandler(powerHandler), m_MemoryManager(memoryManager)
	{
		// ASU RAM chunk - move to ASU class later
		{
			emu::MemoryChunk chunk{};
			chunk.StartAddress = 0x0000;
			chunk.Size = 0x0010;
			chunk.Owner = emu::MemoryOwner::ASU;
			chunk.Type = emu::MemoryType::RAM;
			chunk.Name = "ASU RAM";

			memoryManager.AddChunk(chunk);
		}

	}


	auto APU::Execute() -> void
	{
		m_Executing.store(true);

		while (m_Executing.load())
		{
			if (m_PowerHandler.GetState() == PowerState::SingleStep || m_PowerHandler.GetState() == PowerState::Off)
				m_PowerHandler.SetState(PowerState::Suspended);
			{
				std::unique_lock<std::mutex> lock(m_Mutex);
				m_CV.wait(lock, [&] {
					return m_PowerHandler.GetState() != PowerState::Suspended || m_Executing.load() == 0;
					});


			}
		}

	}

	auto APU::Stop() -> void
	{
		m_Executing.store(false);
		m_CV.notify_all();
	}

	auto APU::UpdatePowerState() -> void
	{
		m_CV.notify_all();
	}

}
