#include <windows.h>

int
main()
{
	Sleep(1);
	GetTickCount();

	LARGE_INTEGER x;
	QueryPerformanceFrequency(&x);

	HMIDIOUT h;
	midiOutOpen(&h, 0, 0, 0, 0);
}
