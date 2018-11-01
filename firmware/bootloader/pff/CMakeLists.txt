add_library (pff
	diskio.h
	pffconf.h
	pff.h
	integer.h

	diskio.c
	pff.c
)

add_custom_command (TARGET pff
	POST_BUILD
	COMMENT "Calculating size of the library"
	COMMAND ${CMAKE_SIZE} $<TARGET_FILE:pff>
)

