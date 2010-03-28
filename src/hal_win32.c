/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#include "config.h"

#ifdef HAL_WIN32

#include <windows.h>
#undef TRUE
#undef FALSE
#undef ERROR

#include <stdio.h>
#include "vomid_local.h"

static void
sleep_cleanup()
{
	timeEndPeriod(1);
}

void
sleep(systime_t t)
{
	static int initialized = 0;
	if (!initialized) {
		timeBeginPeriod(1);
		atexit(sleep_cleanup);
		initialized = 1;
	}
	Sleep((DWORD)(t * 1000));
}

systime_t
systime()
{
	//TODO: QueryPerformanceCounter
	return 0.001 * GetTickCount();
}

static void
_enum_devices(int type, device_clb_t clb, void *arg)
{
	if (type == OUTPUT_DEVICE) {
		int n = midiOutGetNumDevs();
		for (int i = -1; i < n; i++) {
			char id[64];
			MIDIOUTCAPS caps;
			sprintf(id, "win32/%i", i);
			if (midiOutGetDevCaps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
				clb(id, caps.szPname, arg);
		}
	}
}

static HMIDIOUT hout = NULL;

static status_t
_set_device(int type, const char *id)
{
	if (type == OUTPUT_DEVICE) {
		HMIDIOUT h;
		int n = atoi(id);
		if (midiOutOpen(&h, n, 0, 0, 0) == MMSYSERR_NOERROR) {
			if (hout != NULL)
				midiOutClose(hout);
			hout = h;
			return OK;
		}
	}
	return ERROR;
}

static void
_output(const uchar *ev, size_t size)
{
	if (hout != NULL) {
		DWORD msg = 0;
		while (size--)
			msg = msg * 0x100 + ev[size];
		midiOutShortMsg(hout, msg);
	}
}

platform_t vmd_platform_winmm = {
	.enum_devices = _enum_devices,
	.set_device = _set_device,
	.output = _output,
	.name = "win32"
};

#endif // HAL_WIN32
