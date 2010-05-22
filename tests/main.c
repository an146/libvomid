#include <string.h> /* strcmp */

#define SETUP void setup()
#define TEARDOWN void teardown()
#define TEST(name) void test_ ## name ()
#include "tests.h"
#undef SETUP
#undef TEARDOWN
#undef TEST

int
main(int argc, char **argv)
{
	if (argc < 2)
		return 1;

#define SETUP setup()
#define TEARDOWN teardown()
#define TEST(name) if (!strcmp(argv[1], #name)) test_ ## name ()
#include "tests.h"
#undef SETUP
#undef TEARDOWN
#undef TEST
}
