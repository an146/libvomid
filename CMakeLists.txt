include (cmake/process_tests.cmake)
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

set (TESTS
	test/bst.c
	test/file.c
	test/import.c
	test/map.c
	test/native-check.c
	test/track.c
)

set (TEST_SOURCES
	test/tester.c
	${TESTS}
)

include_directories ("include")

process_tests ()
require_c99 ()
find_package(ALSA)
set (HAL_ALSA ${ALSA_FOUND})
set (HAL_WINMM ${WIN32})
if (UNIX AND NOT WIN32)
	set (HAL_POSIX TRUE)
else ()
	set (HAL_POSIX FALSE)
endif ()

configure_file (
	"${PROJECT_SOURCE_DIR}/src/config.h.in"
	"${PROJECT_BINARY_DIR}/config.h"
)
include_directories (${PROJECT_BINARY_DIR} ${ALSA_INCLUDE_DIR})

add_library (libvomid ${SOURCES})
target_link_libraries (libvomid ${ALSA_LIBRARY})
set_target_properties (libvomid PROPERTIES OUTPUT_NAME vomid)
add_executable (tester ${TEST_SOURCES})
target_link_libraries (tester libvomid)