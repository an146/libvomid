#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "vomid_test.h"

void
vmd_test_fail(const char *file, int line, const char *fmt, ...)
{
	va_list va;

	fprintf(stderr, "%s:%i: ", file, line);

	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);

	exit(1);
}

static char *
get_suite_name(const char *test_name)
{
	const char *i = strchr(test_name, '/');

	if (i == NULL)
		i = test_name + strlen(test_name);

	int len = i - test_name;

	char *ret = malloc(len + 1);
	strncpy(ret, test_name, len);
	ret[len] = '\0';

	return ret;
}

typedef void (*test_t)();

#define SET_ROUTINE(src, dest, name1, name2) \
	void src(); \
	if (name1 != NULL && name2 != NULL && !strcmp(name1, name2)) { \
		dest = src; \
	}

int
main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s test-name | %s --list\n", argv[0], argv[0]);
		exit(1);
	}

	if (!strcmp(argv[1], "--list")) {

#undef TEST
#define TEST(suite, name) printf("%s/%s\n", #suite, #name);

#undef TEST_SETUP
#define TEST_SETUP(suite)

#undef TEST_TEARDOWN
#define TEST_TEARDOWN(suite)

#define TEST_SUITE(suite)

#include "reg_tests.h"

		exit(0);
	}

	const char *test_name = argv[1];
	char *suite_name = get_suite_name(test_name);

	test_t setup = NULL, test = NULL, teardown = NULL;

#undef TEST
#define TEST(suite, name) SET_ROUTINE(JOIN3(test, suite, name), test, #suite "/" #name, test_name)

#undef TEST_SETUP
#define TEST_SETUP(suite) SET_ROUTINE(JOIN(test_setup, suite), setup, #suite, suite_name)

#undef TEST_TEARDOWN
#define TEST_TEARDOWN(suite) SET_ROUTINE(JOIN(test_teardown, suite), teardown, #suite, suite_name)

#include "reg_tests.h"

	srand(0x12345678); /* for reproducability */

	if (setup)
		setup();

	if (test) {
		test();
	} else {
		fprintf(stderr, "No such test\n");
		exit(1);
	}

	if (teardown)
		teardown();

	free(suite_name);

	return 0;
}
