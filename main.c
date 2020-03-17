#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> // For PATH_MAX
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "util.h"

const char *usage =
"Usage: %s <directory> <command> [args...]\n"
"\n"
"  Run <directory> as a container and execute <command>.\n";

char * const envp[] = {
    "TERM=xterm-256color",
    "HOME=/root",
    "PWD=/",
    "container=systemd-nspawn",
    NULL
};

typedef struct _SpawnConfig {
    char **argv;
} SpawnConfig;

int child(SpawnConfig *config) {
    char **argv = config->argv;

    // Determine TTY
    char ttypath[PATH_MAX];
    ssize_t ttypathlen = determine_tty(ttypath, sizeof ttypath);

    if (ttypathlen <= 0) {
        fprintf(stderr, "Failed to determine TTY\n");
        return 1;
    }
    //fprintf(stderr, "TTY: %s\n", ttypath);

    // Learned from systemd-nspawn
    mount(NULL, "/", NULL, MS_REC | MS_SLAVE, NULL);
    mount(".", ".", NULL, MS_BIND | MS_REC, NULL);
    mount(NULL, ".", NULL, MS_REC | MS_SHARED, NULL);

    // Mount necessary stuff
    mount("none", "proc", "proc", 0, NULL);
    mount("none", "sys", "sysfs", MS_RDONLY, NULL);
    mount("none", "tmp", "tmpfs", 0, NULL);
    mount("none", "dev", "tmpfs", MS_PRIVATE, NULL);

    // Create device nodes
    mknod_chown("dev/tty", S_IFCHR | 0666, makedev(5, 0), 0, 5);
    mknod_chown("dev/console", S_IFCHR | 0666, makedev(5, 1), 0, 5);
    mknod_chown("dev/ptmx", S_IFCHR | 0666, makedev(5, 2), 0, 5);
    mknod("dev/null", S_IFCHR | 0666, makedev(1, 3));
    mknod("dev/zero", S_IFCHR | 0666, makedev(1, 5));
    mknod("dev/random", S_IFCHR | 0666, makedev(1, 8));
    mknod("dev/urandom", S_IFCHR | 0666, makedev(1, 9));
    mount(ttypath, "dev/console", "", MS_BIND, NULL);

    // Chroot after mounting
    if (chroot(".") == -1) {
        perror("chroot");
        return 1;
    }

    // Execute the target command
    execvpe(argv[0], argv, envp);
    perror("exec");
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, usage, argv[0]);
        return 1;
    }
    if (chdir(argv[1]) == -1) {
        perror(argv[1]);
        return 1;
    }
    // Prepare stack for child
    void *child_stack = malloc(1048576);
    void *child_stack_start = child_stack + 1048576 - 1;

    SpawnConfig config = {
        .argv = argv + 2
    };
    pid_t pid = clone((int(*)(void*))child, child_stack_start,
                      SIGCHLD | // Without SIGCHLD in flags we can't wait(2) for it
                      CLONE_NEWCGROUP | CLONE_NEWIPC | CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS,
                      &config);
    if (pid == -1) {
        perror("clone");
        return 1;
    }

    // Parent waits for child
    int status;
    wait(&status);
    if (WIFEXITED(status)) {
        printf("Exited with status %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("Killed by signal %d\n", WTERMSIG(status));
    }

    // Cleanup - reverse order of mounting
    umount("dev/console");
    umount("dev");
    umount("tmp");
    umount("sys");
    umount("proc");

    return 0;
}
