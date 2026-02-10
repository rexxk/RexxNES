#pragma once

#include <cstdint>
#include <span>
#include <vector>


namespace emu
{


	class ROM
	{
	public:
		ROM() = default;
		ROM(std::uint16_t size, std::span<std::uint8_t> data);

		auto SetData(std::uint16_t size, std::span<std::uint8_t> data) -> void;
		auto GetData() -> std::span<std::uint8_t>;

	private:
		std::uint16_t m_Size{};
		std::vector<std::uint8_t> m_Data{};
	};


}
