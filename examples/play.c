/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdarg.h>
#include <vomid.h>

static void
die(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
	fprintf(stderr, "\n");

	exit(1);
}

static void
event_clb(unsigned char *ev, size_t size, void *arg)
{
	vmd_output(ev, size);
}

static vmd_status_t
delay_clb(vmd_time_t delay, int tempo, void *_file)
{
	vmd_file_t *file = _file;
	vmd_flush_output();
	vmd_sleep(vmd_time2systime(delay, tempo, file->division));
	return VMD_OK;
}

static void
enum_clb(const char *id, const char *name, void *arg)
{
	static int device_set = 0;
	if (!device_set && vmd_set_device(VMD_OUTPUT_DEVICE, id) == VMD_OK) {
		printf("* %s\t%s\n", id, name);
		device_set = 1;
	} else
		printf("  %s\t%s\n", id, name);
}

int
main(int argc, char **argv)
{
	if (argc < 2)
		die("Usage: %s file.mid", argv[0]);

	vmd_file_t file;
	vmd_bool_t native;
	if (vmd_file_import(&file, argv[1], &native) != VMD_OK)
		die("File import failed");

	printf("This is vomid-produced file.");
	if (!native)
		printf(" NOT!!");
	printf("\n");

	vmd_enum_devices(VMD_OUTPUT_DEVICE, enum_clb, NULL);
	vmd_file_play(&file, 0, event_clb, delay_clb, &file);
	vmd_file_fini(&file);
}
