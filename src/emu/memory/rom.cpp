#include "emu/memory/rom.h"



namespace emu
{


	ROM::ROM(std::uint16_t size, std::span<std::uint8_t> data)
		: m_Size(size)
	{
		SetData(size, data);
	}

	auto ROM::SetData(std::uint16_t size, std::span<std::uint8_t> data) -> void
	{
		m_Size = size;
		m_Data.resize(size);

		std::copy(data.begin(), data.end(), m_Data.data());
	}

	auto ROM::GetData() -> std::span<std::uint8_t>
	{
		return m_Data;
	}

}
