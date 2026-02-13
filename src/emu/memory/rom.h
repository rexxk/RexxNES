#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>


namespace emu
{

	enum class ROMType
	{
		Program,
		Character,
	};


	class ROM
	{
	public:
		ROM() = default;
		ROM(const ROM& other);
		ROM(std::uint16_t size, std::span<std::uint8_t> data);

		auto SetData(std::uint16_t size, std::span<std::uint8_t> data) -> void;
		auto GetData() -> auto& { return m_Data; }

		auto GetSize() const -> auto const { return m_Size; }

	private:
		std::uint16_t m_Size{};
		std::vector<std::uint8_t> m_Data{};
	};


}
