#include "vomid_test.h"

file_t file;

TEST("native-check")
{
	vmd_bool_t native;
	if (file_import_f(&file, stdin, &native) != OK)
		printf("invalid\n");
	else if (!native)
		printf("non-native\n");
	else
		printf("native\n");
}
