//===========================================================================

#include <sys/ioctl.h>

#include <string.h>

#include "loop_info.h"
#include "loop.h"

//===========================================================================

int losetup (
	int loopfd,
	int filefd,
	const char *filename
) {
	int r; struct loop_info64 lo;

	r = ioctl(loopfd, LOOP_SET_FD, filefd);
	if (r < 0) return r;

	memset(&lo, 0, sizeof(lo));
	strncpy((char *)lo.lo_file_name, filename, LO_NAME_SIZE - 1);

	return ioctl(loopfd, LOOP_SET_STATUS64, &lo);
}

//===========================================================================

int lodelete (int loopfd) {
	return ioctl(loopfd, LOOP_CLR_FD, 0);
}

//===========================================================================

