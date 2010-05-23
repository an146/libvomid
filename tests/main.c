#include "vomid_test.h"
#include <stdarg.h>
#include <stdio.h>  /* fprintf */
#include <stdlib.h> /* exit */
#include <string.h> /* strcmp */

#define SETUP void setup()
#define TEARDOWN void teardown()
#define TEST(name) void test_ ## name ()
#include "tests.h"
#undef SETUP
#undef TEARDOWN
#undef TEST

void
fail(const char *file, int line, const char *fmt, ...)
{
	va_list va;

	fprintf(stderr, "%s:%i: ", file, line);

	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);

	exit(1);
}

int
main(int argc, char **argv)
{
	if (argc < 2)
		return 1;

	if (argc >= 3) {
		FILE *in_file = freopen(argv[2], "r", stdin);
		if (in_file == NULL) {
			fprintf(stderr, "failed to open: %s\n", argv[2]);
			ASSERT(in_file != NULL);
		}
	}
			

#define SETUP setup()
#define TEARDOWN teardown()
#define TEST(name) if (!strcmp(argv[1], #name)) test_ ## name ()
#include "tests.h"
#undef SETUP
#undef TEARDOWN
#undef TEST
}
