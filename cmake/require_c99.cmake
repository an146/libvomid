macro (require_c99)
	# enabling c99 in gcc
	string (REGEX MATCH "gcc(\\.exe)?$" GCC "${CMAKE_C_COMPILER}")
	if (GCC)
		set (CMAKE_C_FLAGS "-std=c99")
	endif ()

	if ("${C99_SUPPORTED}" STREQUAL "")
		message (STATUS "Checking for C99 support")
		file (WRITE ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/test_c99.c
			"#if !defined(__STDC_VERSION__)\n"
			"#error __STDC_VERSION__ must be defined\n"
			"#endif\n"
			"#if __STDC_VERSION__ < 199901L\n"
			"#error we need c99, but current std version is __STDC_VERSION__\n"
			"#endif\n"
			"int main()\n"
			"{return 0;}\n")
		try_compile (C99_SUPPORTED ${CMAKE_BINARY_DIR}
			${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/test_c99.c) 
	endif ()
	if (C99_SUPPORTED)
		message (STATUS "C99 supported")
	else ()
		message (FATAL_ERROR "C99 not supported")
	endif ()
endmacro ()
