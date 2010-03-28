/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#include <string.h>
#include <stdlib.h>
#include "vomid_local.h"

static platform_t *platforms;
static platform_t *cur_platform[DEVICE_TYPES] = {};

void
sleep_till(systime_t t)
{
	sleep(t - systime());
	while (systime() < t)
		;
}

#define ADD_PLATFORM(p) do { p->next = platforms; platforms = p; } while(0)
#define FOR_EACH_PLATFORM(p) for (p = platforms; p != NULL; p = p->next)

static void
fini_platforms()
{
	notes_off();

	platform_t *p;
	FOR_EACH_PLATFORM(p) {
		if (!p->initialized && p->fini != NULL)
				p->fini();
	}
}

static void
init_platforms()
{
	static int initialized = 0;
	if (!initialized) {
		atexit(fini_platforms);
#ifdef HAVE_ALSA
		ADD_PLATFORM(platform_alsa);
#endif
#ifdef WINDOWS
		ADD_PLATFORM(platform_winmm);
#endif
		initialized = 1;
	}

	platform_t *p;
	FOR_EACH_PLATFORM(p) {
		if (!p->initialized && (p->init == NULL || p->init() == OK))
			p->initialized = TRUE;
	}
}

void
enum_devices(int type, device_clb_t clb, void *arg)
{
	init_platforms();

	platform_t *p;
	FOR_EACH_PLATFORM(p) {
		if (p->initialized && p->enum_devices != NULL)
			p->enum_devices(type, clb, arg);
	}
}

status_t
set_device(int type, const char *dev)
{
	init_platforms();
	const char *slash = strchr(dev, '/');
	if (!slash)
		return ERROR;

	platform_t *platform = NULL, *p;
	FOR_EACH_PLATFORM(p) {
		if (strlen(p->name) == slash - dev && !strncmp(p->name, dev, slash - dev))
			platform = p;
	}
	if (platform == NULL || platform->set_device == NULL)
		return ERROR;

	status_t ret = platform->set_device(type, slash + 1);
	if (ret == OK) {
		if (cur_platform[type] != NULL && cur_platform[type] != platform)
			cur_platform[type]->set_device(type, NULL);
		cur_platform[type] = platform;
	}
	return ret;
}

void
output(const unsigned char *ev, size_t size)
{
	if (ev[0] < 0x80 || ev[0] >= 0xF0)
		return;
	if (cur_platform[OUTPUT_DEVICE] != NULL && cur_platform[OUTPUT_DEVICE]->output != NULL)
		cur_platform[OUTPUT_DEVICE]->output(ev, size);
}

void
flush_output()
{
	platform_t *p = cur_platform[OUTPUT_DEVICE];
	if (p != NULL && p->flush_output != NULL)
		p->flush_output();
}
