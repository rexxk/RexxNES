#pragma once

#include <cstdint>
#include <vector>


namespace emu
{


	class RAM
	{
	public:
		RAM() = default;
		explicit RAM(std::uint16_t size);

		auto ReadAddress(std::uint16_t address) -> std::uint8_t { return m_Data.at(address); }
		auto WriteAddress(std::uint16_t address, std::uint8_t value) -> void { m_Data.at(address) = value; }

		auto GetAddress(std::uint16_t address) -> std::uint8_t& { return m_Data.at(address); }

		auto GetData() -> auto& { return m_Data; }

	private:
		std::uint16_t m_Size{};

		std::vector<std::uint8_t> m_Data{};
	};


}
