
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#ifndef MS_MOVE
#define MS_MOVE 8192
#endif

#include "debug.h"
#include "loop.h"

/* Split the passed buffer as a list of parameters. */
static int __mkparam (char *buf, char **paramv, int maxparam, const char delim)
{
	int paramc;

	paramv[0] = strtok(buf, &delim);
	if (!paramv[0])
		return 0;

	for (paramc=1; ; paramc++) {
		paramv[paramc] = strtok(NULL, &delim);
		if (!paramv[paramc]) break;
	}

	return paramc;
}

static int __read_text_file (const char *fn, char *buf, size_t len)
{
	int fd, r;
	fd = open(fn, O_RDONLY);
	if (fd < 0) {
		ERROR("Unable to open \'%s\'.\n", fn);
		return fd;
	}

	r = read(fd, buf, len-1);
	close(fd);
	if (r < 0) {
		ERROR("Unable to read \'%s\'.\n", fn);
		return r;
	}

	/* Skip the last \n */
	if (r && (buf[r-1] == '\n'))
		buf[r-1] = '\0';

	buf[r] = '\0';
	return 0;
}

static int __mount (
		const char *source,
		const char *target,
		const char *type,
		unsigned long flags,
		const void *data)
{
	int nb;
	char cbuf[4096];
	char * tokens[64];

	if (type || (flags & MS_MOVE))
		return mount(source, target, type, flags, data);

	/* The filesystem is unknown.
	 * We will try each filesystem supported by the kernel. */
	if (__read_text_file("/proc/filesystems", cbuf, sizeof(cbuf)))
		return -1;

	nb = __mkparam(cbuf, tokens, sizeof(tokens)/sizeof(tokens[0]), '\n');

	while (nb--) {
		/* note: the possible filesystems all start with a
		 * tabulation in that file, except ubifs */
		if (!strncmp(tokens[nb], "nodev\tubifs", 11))
			tokens[nb] += 5;
		else if (*tokens[nb] != '\t')
			continue;

		/* skip the tabulation */
		tokens[nb]++;

		if (!mount(source, target, tokens[nb], flags, data))
			return 0;
	}

	DEBUG("Failed attempt to mount %s on %s\n", source, target);
	return -1;
}

static int __multi_mount (
		char *source,
		const char *target,
		const char *type,
		unsigned long flags,
		const void *data,
		int retries)
{
	size_t try;
	char c, *s, *t;

	for (try = 0; try < retries; usleep(100000), try++) {
		for (c = ',', s = source; c == ','; *s++ = c) {
			for (t = s; *s != ',' && *s != '\0'; s++);
			c = *s;
			*s = '\0';

			if (!__mount(t, target, type, flags, data)) {
				INFO("%s mounted on %s\n", t, target);
				*s = c;
				return 0;
			}
		}
	}

	ERROR("Cannot mount %s on %s\n", source, target);
	return -1;
}

static int __losetup (
		const char *loop,
		const char *file)
{
	int loopfd, filefd, res;

	filefd = open(file, O_RDONLY);
	if (filefd < 0) {
		ERROR("losetup: cannot open \'%s\'.\n", file);
		return -1;
	}

	loopfd = open(loop, O_RDONLY);
	if (loopfd < 0) {
		ERROR("losetup: cannot open \'%s\'.\n", file);
		close(filefd);
		return -1;
	}

	res = losetup(loopfd, filefd, file);
	if (res < 0)
		ERROR("Cannot setup loop device \'%s\'.\n", loop);

	close(loopfd);
	close(filefd);
	return res;
}


int main(int argc, char **argv)
{
	char *loop_dev = "/dev/loop0";
	const char *inits [] = {
		"/sbin/init",
		"/etc/init",
		"/bin/init",
		"/bin/sh",
		NULL,
	};

	int fd, boot;

	char sbuf [256];
	char cbuf[4096];
	int paramc;
	char * paramv[64];

	size_t i;

	INFO("\n\n\nOpenDingux min-init 1.1.0 "
				"by Ignacio Garcia Perez <iggarpe@gmail.com> "
				"and Paul Cercueil <paul@crapouillou.net>\n");

	DEBUG("Mounting /proc\n");
	if ( mount(NULL, "/proc", "proc", 0, NULL) ) {
		ERROR("Unable to mount /proc\n");
		return -1;
	}

	DEBUG("Reading kernel command line\n");
	if (__read_text_file("/proc/cmdline", cbuf, sizeof(cbuf)))
		return -1;

	DEBUG("Command line read: %s\n", cbuf);

	/* paramv[0] and paramv[paramc] are reserved */
	paramc = 1 + __mkparam(cbuf, paramv+1, sizeof(paramv)/sizeof(paramv[0]) -2, ' ');

	/* Process "boot" parameter
	 * (only one, allow comma-separated list).
	 * Note that we specify 20 retries (2 seconds), just in case it is
	 * a hotplug device which takes some time to detect and initialize. */
	for (i=1; i<paramc; i++) {
		if (strncmp(paramv[i], "boot=", 5))
			continue;
//		__multi_mount(paramv[i]+5, "/boot", NULL, MS_RDONLY, "utf8", 20);
		if ( __multi_mount(paramv[i]+5, "/boot", NULL, MS_RDONLY, "utf8", 20) )
			return -1;
		boot = 1;
		break;
	}

	if (!boot)
		WARNING("\'boot\' parameter not found.\n");

	/* Process "loop" parameter (multiple) */
	for (i=1; i<paramc; i++) {
		if (strncmp(paramv[i], "loop", 4)
			|| paramv[i][5] != '=') continue;

		/* update the loop device filename if needed */
		loop_dev[9] = paramv[i][4];

		DEBUG("Setting up loopback: \'%s\' associated to \'%s\'.\n",
			loop_dev, paramv[i]+6);
		__losetup(loop_dev, paramv[i]+6);
	}

	/* Process "root" parameter (only one, allow comma-separated list).
	 * Note that we specify 20 retries (2 seconds), just in case it is
	 * a hotplug device which takes some time to detect and initialize. */
	for (i=1; i<paramc; i++) {
		if (strncmp(paramv[i], "root=", 5))
			continue;
		if ( __multi_mount(paramv[i]+5, "/root", NULL, MS_RDONLY, NULL, 20) )
			return -1;
		break;
	}

	/* TODO: try with real-root-dev */
	if (i >= paramc) {
		ERROR("\'root\' parameter not found.\n");
		return -1;
	}

	/* Move the /boot mountpoint so that it is visible
	 * on the new filesystem tree */
	if (boot) {
		DEBUG("Moving \'/boot\' mountpoint\n");
		if ( mount("/boot", "/root/boot", NULL, MS_MOVE, NULL) ) {
			ERROR("Unable to move the \'/boot\' mountpoint.\n");
			return -1;
		}
	}

	/* Now let's switch to the new root */
	DEBUG("Switching root\n");

	if (chdir("/root") < 0) {
		ERROR("Unable to change to \'/root\' directory.\n");
		return -1;
	}

	/* Re-open the console device at the new location
	 * (must already exist). */
	fd = open("/root/dev/console", O_RDWR, 0);
	if (fd < 0) {
		ERROR("Unable to re-open console.\n");
		return -1;
	}

	if ((dup2(fd, 0) != 0)
			|| (dup2(fd, 1) != 1)
			|| (dup2(fd, 2) != 2)) {
		ERROR("Unable to duplicate console handles.\n");
		return -1;
	}
	if (fd > 2) close(fd);

	/* Keep the old root open until the chroot is done */
	fd = open("/", O_RDONLY, 0);

	/* Unmount the previously mounted /proc filesystem. */
	umount("/proc");

	/* Do the root switch */
	if ( mount(".", "/", NULL, MS_MOVE, NULL) ) {
		ERROR("Unable to switch to the new root.\n");
		close(fd);
		return -1;
	}

	if ((chroot(".") < 0) || (chdir("/") < 0)) {
		ERROR("\'chroot\' failed.\n");
		close(fd);
		return -1;
	}

	/* Release the old root */
	close(fd);

	/* Prepare paramv[0] which is the init program itself */
	for (i=1; i<paramc; i++) {
		if (strncmp(paramv[i], "init=", 5))
			continue;
		strcpy(sbuf, paramv[i]+5);
		break;
	}

	/* If no 'init=' is found on the command line, we try to
	 * locate the init program. */
	if (i >= paramc) {
		for (i=0; inits[i] && access(inits[i], X_OK)<0; i++);
		if (!inits[i]) {
			ERROR("Unable to find the \'init\' executable.\n");
			return -1;
		}
		strcpy(sbuf, inits[i]);
	}

	paramv[0] = sbuf;

	/* Execute the 'init' executable */
	execv(paramv[0], paramv);
	ERROR("exec or init failed.\n");
	return 0;
}
