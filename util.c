#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h> // For PATH_MAX

ssize_t determine_tty(char *ttypath, size_t bufsize) {
    char fdpath[PATH_MAX] = {};
    ssize_t len;

    // Attempt to determine stdin TTY
    snprintf(fdpath, sizeof fdpath, "/proc/self/fd/%d", STDIN_FILENO);
    len = readlink(fdpath, ttypath, bufsize);
    if (len > 0 && (!strncmp(ttypath, "/dev/tty", 8) || !strncmp(ttypath, "/dev/pts/", 9))) {
        // Success on stdin
        ttypath[len] = '\0';
        return len;
    }

    if (len == -1) {
        fprintf(stderr, "Failed to determine stdin from /proc/self/fd/%d: %s\n", STDIN_FILENO, strerror(errno));
    } else if (strncmp(ttypath, "/dev/tty", 8) && strncmp(ttypath, "/dev/pts/", 9)) {
        fprintf(stderr, "stdin does not appear to be a TTY: %s\n", ttypath);
    }

    // Try again with stderr
    snprintf(fdpath, sizeof fdpath, "/proc/self/fd/%d", STDERR_FILENO);
    len = readlink(fdpath, ttypath, bufsize);
    if (len > 0 && (!strncmp(ttypath, "/dev/tty", 8) || !strncmp(ttypath, "/dev/pts/", 9))) {
        // Success on stderr
        ttypath[len] = '\0';
        return len;
    }

    if (len == -1) {
        fprintf(stderr, "Failed to determine stderr from /proc/self/fd/%d: %s\n", STDERR_FILENO, strerror(errno));
    } else if (strncmp(ttypath, "/dev/tty", 8) && strncmp(ttypath, "/dev/pts/", 9)) {
        // Failed
        fprintf(stderr, "stderr does not appear to be a TTY: %s\n", ttypath);
    }
    return 0;
}

int mknod_chown(const char *path, mode_t mode, dev_t dev, uid_t uid, gid_t gid) {
    if (mknod(path, mode, dev) == -1)
        return -1;
    if (chown(path, uid, gid) == -1)
        return -1;
    return 0;
}

int read_file(const char *path, char *content, size_t size) {
    int fd = open(path, O_RDONLY);
    if (fd == -1)
        return -1;
    int ret = read(fd, content, size);
    int saved_errno = errno;
    close(fd);
    errno = saved_errno;
    return ret;
}

int write_file(const char *path, const char *content) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd == -1)
        return -1;
    int ret = write(fd, content, strlen(content));
    int saved_errno = errno;
    close(fd);
    errno = saved_errno;
    return ret;
}
