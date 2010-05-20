include (cmake/require_c99.cmake)

cmake_minimum_required (VERSION 2.8)
project (libvomid)

set (SOURCES
	src/3rdparty/sha1/sha1.c
	src/bst.c
	src/channel.c
	src/export.c
	src/file.c
	src/hal_alsa.c
	src/hal.c
	src/hal_posix.c
	src/hal_win32.c
	src/import.c
	src/map.c
	src/midi.c
	src/note.c
	src/notesystem.c
	src/play.c
	src/pool.c
	src/stack.c
	src/track.c
)

include_directories ("include")

require_c99 ()
find_package(ALSA)
set (HAL_ALSA ${ALSA_FOUND})
set (HAL_WIN32 ${WIN32})
if (UNIX AND NOT WIN32)
	set (HAL_POSIX TRUE)
else ()
	set (HAL_POSIX FALSE)
endif ()

configure_file (
	"${PROJECT_SOURCE_DIR}/src/config.h.in"
	"${PROJECT_BINARY_DIR}/config.h"
)
include_directories (${PROJECT_BINARY_DIR})

add_library (libvomid ${SOURCES})
set_target_properties (libvomid PROPERTIES OUTPUT_NAME vomid)

if (HAL_ALSA)
	include_directories (${ALSA_INCLUDE_DIR})
	target_link_libraries (libvomid ${ALSA_LIBRARY})
endif ()

if (HAL_WIN32)
	target_link_libraries (libvomid winmm)
endif ()

add_executable (play examples/play.c)
target_link_libraries (play libvomid)
