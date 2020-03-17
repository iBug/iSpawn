#include "cgroup.h"

#include <errno.h>
#include <fcntl.h>
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

int mount_cgroup(void) {
    int cgmountflags = MS_NOSUID | MS_NODEV | MS_NOEXEC;
    // Mount a tmpfs first
    mount("none", "sys/fs/cgroup", "tmpfs", cgmountflags, "mode=755");
    cgmountflags |= MS_RELATIME;

    // Prepare mount points
    mkdir("sys/fs/cgroup/blkio", 0755);
    mkdir("sys/fs/cgroup/cpu,cpuacct", 0755);
    mkdir("sys/fs/cgroup/memory", 0755);
    mkdir("sys/fs/cgroup/pids", 0755);

    // Mount cgroup subsystems
    mount("cgroup", "sys/fs/cgroup/blkio", "cgroup", cgmountflags, "blkio");
    mount("cgroup", "sys/fs/cgroup/cpu,cpuacct", "cgroup", cgmountflags, "cpu,cpuacct");
    mount("cgroup", "sys/fs/cgroup/memory", "cgroup", cgmountflags, "memory");
    mount("cgroup", "sys/fs/cgroup/pids", "cgroup", cgmountflags, "pids");

    // cpu and cpuacct need symlinks
    symlink("cpu,cpuacct", "sys/fs/cgroup/cpu");
    symlink("cpu,cpuacct", "sys/fs/cgroup/cpuacct");

    // Remount the tmpfs as R/O
    mount(NULL, "sys/fs/cgroup", NULL, MS_REMOUNT | MS_RDONLY | cgmountflags, NULL);
    return 0;
}

int set_cgroup(pid_t pid) {
    // Memory
    mkdir("/sys/fs/cgroup/memory/ispawn", 0755);
    write_file("/sys/fs/cgroup/memory/ispawn/memory.limit_in_bytes", "1073741824");
    write_file("/sys/fs/cgroup/memory/ispawn/memory.kmem.limit_in_bytes", "1073741824");
    write_file("/sys/fs/cgroup/memory/ispawn/memory.swappiness", "0");

    // CPU
    mkdir("/sys/fs/cgroup/cpu/ispawn", 0755);
    write_file("/sys/fs/cgroup/cpu/ispawn/cpu.shares", "256");

    // PIDs
    mkdir("/sys/fs/cgroup/pids/ispawn", 0755);
    write_file("/sys/fs/cgroup/pids/ispawn/pid.max", "256");

    // Block I/O
    mkdir("/sys/fs/cgroup/blkio/ispawn", 0755);
    write_file("/sys/fs/cgroup/blkio/ispawn/weight", "50");

    // Apply settings
    char s[10];
    sprintf(s, "%d", pid);
    write_file("/sys/fs/cgroup/memory/ispawn/cgroup.procs", s);
    write_file("/sys/fs/cgroup/cpu/ispawn/cgroup.procs", s);
    write_file("/sys/fs/cgroup/pids/ispawn/cgroup.procs", s);
    write_file("/sys/fs/cgroup/blkio/ispawn/cgroup.procs", s);
}

int reset_cgroup() {
    // Not expecting this buffer to fill up
    size_t bufsize = 2048;
    char *buf = malloc(bufsize);

    // Move all processes back to root cgroup
    read_file("/sys/fs/cgroup/memory/ispawn/cgroup.procs", buf, bufsize);
    write_file("/sys/fs/cgroup/memory/cgroup.procs", buf);
    rmdir("/sys/fs/cgroup/memory/ispawn");

    read_file("/sys/fs/cgroup/cpu/ispawn/cgroup.procs", buf, bufsize);
    write_file("/sys/fs/cgroup/cpu/cgroup.procs", buf);
    rmdir("/sys/fs/cgroup/cpu/ispawn");

    read_file("/sys/fs/cgroup/blkio/ispawn/cgroup.procs", buf, bufsize);
    write_file("/sys/fs/cgroup/blkio/cgroup.procs", buf);
    rmdir("/sys/fs/cgroup/blkio/ispawn");

    read_file("/sys/fs/cgroup/pids/ispawn/cgroup.procs", buf, bufsize);
    write_file("/sys/fs/cgroup/pids/cgroup.procs", buf);
    rmdir("/sys/fs/cgroup/pids/ispawn");

    free(buf);
    return 0;
}
