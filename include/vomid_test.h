/* (C)opyright 2008 Anton Novikov
 * See LICENSE file for license details.
 *
 * test.h
 * simple testing framework
 */

#ifndef TEST_H
#define TEST_H

#include "vomid_local.h"

#define TEST(name) \
	void \
	JOIN3(test, FILE_ID, __LINE__)()

#define TEST_SETUP(name) \
	void \
	JOIN3(test, FILE_ID, __LINE__)()

#define TEST_TEARDOWN(name) \
	void \
	JOIN3(test, FILE_ID, __LINE__)()

void vmd_test_fail(const char *file, int line, const char *fmt, ...);

#define FAIL(...) vmd_test_fail(__FILE__, __LINE__, __VA_ARGS__)

//TODO: this sucks...
#define ASSERT(x) \
	if (!(x)) { \
		FAIL("Assertion '"#x"' failed\n"); \
	}

#define ASSERT_EQ(x, y, fmt) \
	if ((x) != (y)) { \
		FAIL("Assertion '"#x" == "#y"' failed: " fmt " != " fmt "\n", x, y); \
	}

#define ASSERT_EQ_INT(x, y) ASSERT_EQ(x, y, "%i")

void vmd_map_print(map_t *);

#endif /* TEST_H */
