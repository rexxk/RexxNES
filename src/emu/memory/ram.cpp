#include "emu/memory/ram.h"



namespace emu
{

	RAM::RAM(std::uint16_t size)
		: m_Size(size)
	{
		m_Data.resize(size);
	}


}
