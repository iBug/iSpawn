#ifndef __UTIL_H
#define __UTIL_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>

ssize_t determine_tty(char *, size_t bufsize);
int mknod_chown(const char *, mode_t, dev_t, uid_t, gid_t);

#endif
