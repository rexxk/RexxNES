#pragma once

#include <cstdint>
#include <vector>


namespace emu
{


	class RAM
	{
	public:
		explicit RAM(std::uint16_t size);



	private:
		std::uint16_t m_Size{};

		std::vector<std::uint8_t> m_Data{};
	};


}
