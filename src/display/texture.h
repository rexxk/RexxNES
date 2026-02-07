#pragma once

#include <cstdint>
#include <span>

#include <glad/glad.h>


namespace emu
{

	class Texture
	{
	public:
		Texture(std::uint16_t width, std::uint16_t height);
		~Texture();

		auto SetData(std::span<std::uint8_t> colorData) -> void;

		auto GetTexture() -> GLuint { return m_TextureID; }

	private:
		GLuint m_TextureID{};

		std::uint16_t m_Width{};
		std::uint16_t m_Height{};
	};

}
