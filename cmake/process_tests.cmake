include (${LIBVOMID_DIR}/cmake/parse_arguments.cmake)
macro (process_tests)
	get_filename_component (SUITE ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

	parse_arguments (TEST "SOURCES;INPUTS" "" ${ARGN})
	set (TESTS_HEADER ${CMAKE_CURRENT_BINARY_DIR}/tests.h)
	include_directories (${CMAKE_CURRENT_BINARY_DIR})

	add_executable (${SUITE}-tester ${TEST_SOURCES} ../main.c)
	target_link_libraries (${SUITE}-tester libvomid)

	file (REMOVE ${TESTS_HEADER})
	list (FIND TEST_SOURCES setup.c SETUP_POS)
	list (FIND TEST_SOURCES teardown.c TEARDOWN_POS)
	list (REMOVE_ITEM TEST_SOURCES common.c setup.c teardown.c)

	if (NOT ${SETUP_POS} EQUAL -1)
		file (APPEND ${TESTS_HEADER} "SETUP;\n")
	endif ()
	foreach (TEST_SOURCE ${TEST_SOURCES})
		string (REGEX REPLACE "\\.c$" "" TEST ${TEST_SOURCE})
		file (APPEND ${TESTS_HEADER} "TEST(${TEST});\n")
		list (APPEND TESTS ${TEST})
	endforeach()
	if (NOT ${TEARDOWN_POS} EQUAL -1)
		file (APPEND ${TESTS_HEADER} "TEARDOWN;\n")
	endif ()

	foreach (TEST ${TESTS})
		if (DEFINED TEST_INPUTS)
			foreach (INPUT ${TEST_INPUTS})
				string (REGEX REPLACE "\\.in$" "" INP ${INPUT})
				get_filename_component (INP ${INPUT} NAME_WE)
				add_test ("${SUITE}/${TEST}/${INP}" ${SUITE}-tester ${TEST} ${CMAKE_CURRENT_SOURCE_DIR}/${INPUT})
			endforeach()
		else ()
			add_test (${SUITE}/${TEST} ${SUITE}-tester ${TEST})
		endif ()
	endforeach ()
endmacro()