#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> // For PATH_MAX
#include <unistd.h>
#include <sched.h>
#include <sys/mount.h>
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
    NULL
};

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, usage, argv[0]);
        return 1;
    }
    if (chdir(argv[1]) == -1) {
        perror(argv[1]);
        return 1;
    }
    if (unshare(CLONE_NEWCGROUP | CLONE_NEWIPC | CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS) == -1) {
        perror("unshare");
        return 1;
    }
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        // Child is in new PID namespace

        // Determine TTY
        char ttypath[PATH_MAX];
        ssize_t ttypathlen = determine_tty(ttypath, sizeof ttypath);

        if (ttypathlen <= 0) {
            fprintf(stderr, "Failed to determine TTY\n");
            return 1;
        }
        //fprintf(stderr, "TTY: %s\n", ttypath);

        // Mount necessary stuff
        mount("none", "proc", "proc", 0, NULL);
        mount("none", "sys", "sysfs", MS_RDONLY, NULL);
        mount("none", "dev", "tmpfs", MS_PRIVATE, NULL);

        // Bind mount target must exist
        creat("dev/console", 0755);
        mount(ttypath, "dev/console", "", MS_BIND, NULL);

        // Chroot after mounting
        if (chroot(".") == -1) {
            perror("chroot");
            return 1;
        }

        // Execute the target command
        execvpe(argv[2], argv + 2, envp);
        perror("exec");
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
    umount("sys");
    umount("proc");

    return 0;
}
