/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#ifdef HAL_POSIX

#define _BSD_SOURCE
#include <sys/time.h> /* gettimeofday */
#include <unistd.h> /* usleep */
#include "vomid_local.h"

void
sleep(systime_t t)
{
	//TODO: nanosleep? usleep isn't in latest POSIX std
	usleep((unsigned int)(t * 1000 * 1000));
}

systime_t
systime()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec + (systime_t)tv.tv_usec / (1000 * 1000);
}

platform_t vmd_platform_posix = {
	.name = "posix"
};

#endif // HAL_POSIX
