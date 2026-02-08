# RexxNES/src


add_subdirectory(display)
add_subdirectory(emu)
add_subdirectory(input)


target_sources(RexxNES PRIVATE
	main.cpp
)
