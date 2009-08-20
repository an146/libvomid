#include <sys/time.h>

#ifdef WIN32
#   error Nonononononono
#endif

int
main()
{
	struct timeval t1;
	gettimeofday(&t1,0);
}
