
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <signal.h>
#include <linux/input.h>

#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "debug.h"

#define MAGIC_KEY KEY_ENTER

static char * const process_name = "splk";
static char * const init_fn = "/mininit";
static char * const event_fn = "/dev/event0";
static char * const console_fn = "/sys/devices/virtual/vtconsole/vtcon1/bind";
static char * const sys_mountpoint = "/sys";

static int fd;

static void quit_hdl(int err)
{
	close(fd);
	exit(err);
}

static int waitForEnter()
{
	struct input_event event;

	fd = open(event_fn, O_RDONLY);
	if (fd < 0) {
		ERROR("Unable to open %s\n", event_fn);
		return -1;
	}

	do {
		while(read(fd, &event, sizeof(event)) < sizeof(event));
	} while (event.code != MAGIC_KEY);
	close(fd);
	return 0;
}

int main(int argc, char **argv)
{
	int res, argv0size = strlen(argv[0]);
	char one = '1';

	signal(SIGINT,  &quit_hdl);
	signal(SIGSEGV, &quit_hdl);
	signal(SIGTERM, &quit_hdl);

	if (fork())
		execl(init_fn, "/init", NULL);

	strlcpy(argv[0], process_name, argv0size);

	res = waitForEnter();
	if (res)
		return res;

	if (mount(NULL, sys_mountpoint, "sysfs", 0, NULL)) {
		ERROR("Unable to mount sysfs.\n");
		return -1;
	}

	fd = open(console_fn, O_WRONLY);
	if (fd < 0) {
		ERROR("Unable to open %s\n", console_fn);
		return -1;
	}

	write(fd, &one, sizeof(one));

	close(fd);
	if (umount(sys_mountpoint)) {
		ERROR("Unable to un-mount sysfs.\n");
		return -1;
	}
	return 0;
}
