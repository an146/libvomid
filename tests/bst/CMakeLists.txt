include (../../cmake/process_tests.cmake)

set (INPUTS
	data/ascend.in
	data/descend.in
	data/nothing.in
	data/random-1.in
	data/random-2.in
	data/random-3.in
	data/random-4.in
	data/random-5.in
	data/random-dense-1.in
	data/random-dense-2.in
	data/small.in
)

set (SOURCES
	common.c
	setup.c
	teardown.c

	erase.c
	revert.c
	search.c
	traversal.c
	update.c
)

process_tests (INPUTS ${INPUTS} SOURCES ${SOURCES})
