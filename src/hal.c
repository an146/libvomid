/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#include <string.h>
#include <stdlib.h>
#include "vomid_local.h"

void
sleep_till(systime_t t)
{
	sleep(t - systime());
	while (systime() < t)
		;
}

static void
fini_platforms()
{
	notes_off();
#   define PLATFORM(p) \
		if (!p->initialized && p->fini != NULL) \
			p->fini();
#   include "../build/gen/platforms.h"
#   undef PLATFORM
}

static void
init_platforms()
{
	static int initialized = 0;
	if (!initialized) {
		atexit(fini_platforms);
		initialized = 1;
	}
#   define PLATFORM(p) \
		if (!p->initialized && (p->init == NULL || p->init() == OK)) \
			p->initialized = TRUE;
#   include "../build/gen/platforms.h"
#   undef PLATFORM
}

static vmd_platform_t *platforms[DEVICE_TYPES] = {};

void
enum_devices(int type, device_clb_t clb, void *arg)
{
	init_platforms();
#   define PLATFORM(p) \
		if (p->initialized && p->enum_devices != NULL) \
			p->enum_devices(type, clb, arg);
#   include "../build/gen/platforms.h"
#   undef PLATFORM
}

status_t
set_device(int type, const char *dev)
{
	init_platforms();
	const char *slash = strchr(dev, '/');
	if (!slash)
		return ERROR;

	platform_t *platform = NULL;
#   define PLATFORM(p) \
		if (strlen(p->name) == slash - dev && !strncmp(p->name, dev, slash - dev)) \
			platform = p;
#   include "../build/gen/platforms.h"
#   undef PLATFORM
	if (platform == NULL || platform->set_device == NULL)
		return ERROR;

	status_t ret = platform->set_device(type, slash + 1);
	if (ret == OK) {
		if (platforms[type] != NULL && platforms[type] != platform)
			platforms[type]->set_device(type, NULL);
		platforms[type] = platform;
	}
	return ret;
}

void
output(const unsigned char *ev, size_t size)
{
	if (ev[0] < 0x80 || ev[0] >= 0xF0)
		return;
	if (platforms[OUTPUT_DEVICE] != NULL && platforms[OUTPUT_DEVICE]->output != NULL)
		platforms[OUTPUT_DEVICE]->output(ev, size);
}

void
flush_output()
{
	platform_t *p = platforms[OUTPUT_DEVICE];
	if (p != NULL && p->flush_output != NULL)
		p->flush_output();
}
