include (../../cmake/process_tests.cmake)

set (TEST_SOURCES
	common.c
	setup.c
	teardown.c

	traversal.c
	revert.c
)

process_tests (${TEST_SOURCES})
