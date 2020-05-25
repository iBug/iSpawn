#include "fs.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>

#include "util.h"

int prepare_fs(const char *path, char *mounted_path) {
    // Determine TTY
    char ttypath[PATH_MAX];
    ssize_t ttypathlen = determine_tty(ttypath, sizeof ttypath);

    if (ttypathlen <= 0) {
        fprintf(stderr, "Failed to determine TTY\n");
        return 1;
    }
    fprintf(stderr, "Detected TTY: %s\n", ttypath);

    // Remount everything as private
    mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL);

    // Preparations for pivot_root(2)
    char target[] = "/tmp/ispawn.XXXXXX";
    mkdtemp(target);
    strncpy(mounted_path, target, PATH_MAX);
    char put_old[] = "/tmp/ispawn.XXXXXX/oldroot";
    strncpy(put_old, target, strlen(target));
    // Must mount BEFORE creating put_old directory
    mount(path, target, NULL, MS_BIND | MS_PRIVATE, NULL);
    mkdir(put_old, 0755);

    // Create stuff in case they don't pre-exist
    mkdir("dev", 0755);
    mkdir("mnt", 0755);
    mkdir("proc", 0755);
    mkdir("run", 0755);
    mkdir("sys", 0755);
    mkdir("tmp", 0755);

    // Mount necessary stuff
    chdir(target);
    mount("none", "dev", "tmpfs", 0, NULL);
    mount("none", "proc", "proc", 0, NULL);
    mount("none", "run", "tmpfs", 0, NULL);
    mount("none", "sys", "sysfs", MS_RDONLY, NULL);
    mount("none", "tmp", "tmpfs", 0, NULL);

    // Create device nodes
    mknod_chown("dev/tty", S_IFCHR | 0666, makedev(5, 0), 0, 5);
    //mknod_chown("dev/console", S_IFCHR | 0666, makedev(5, 1), 0, 5);
    mknod_chown("dev/ptmx", S_IFCHR | 0666, makedev(5, 2), 0, 5);
    mknod("dev/null", S_IFCHR | 0666, makedev(1, 3));
    mknod("dev/zero", S_IFCHR | 0666, makedev(1, 5));
    mknod("dev/random", S_IFCHR | 0666, makedev(1, 8));
    mknod("dev/urandom", S_IFCHR | 0666, makedev(1, 9));
    //mount(ttypath, "dev/console", "", MS_BIND, NULL);

    // Recreate the same tty as dev/console
    struct stat statbuf;
    stat(ttypath, &statbuf);
    mknod_chown("dev/console", S_IFCHR | 0600, statbuf.st_dev, 0, 5);
    return 0;
}
