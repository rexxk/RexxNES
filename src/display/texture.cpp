#include "display/texture.h"



namespace emu
{

	Texture::Texture(std::uint16_t width, std::uint16_t height)
		: m_Width(width), m_Height(height)
	{
		glCreateTextures(GL_TEXTURE_2D, 1, &m_TextureID);

		glTextureStorage2D(m_TextureID, 1, GL_RGBA8, width, height);
	}

	Texture::~Texture()
	{
		if (m_TextureID)
			glDeleteTextures(1, &m_TextureID);
	}

	auto Texture::SetData(std::span<std::uint8_t> colorData) -> void
	{
		glTextureSubImage2D(m_TextureID, 0, 0, 0, m_Width, m_Height, GL_RGBA, GL_UNSIGNED_BYTE, colorData.data());
	}

}
