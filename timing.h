#ifdef _WIN32
#include <windows.h>
#include <time.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif


static double SysTime()
{
#ifdef _WIN32
	static LARGE_INTEGER lpf;
	LARGE_INTEGER li;

	if (!lpf.QuadPart)
	{
		QueryPerformanceFrequency(&lpf);
	}

	QueryPerformanceCounter(&li);

	return (double)li.QuadPart / (double)lpf.QuadPart;
#else
	struct timeval tv;
	gettimeofday(&tv, 0);
	return ((double)tv.tv_usec) / 1000000. + (tv.tv_sec);
#endif
}