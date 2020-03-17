#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> // For PATH_MAX
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "cap.h"
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
    const char *path;
    char **argv;
    int s, sparent; // Socket communication with parent
} SpawnConfig;

int child(SpawnConfig *config) {
    char **argv = config->argv;
    if (config->sparent)
        close(config->sparent);
    FILE *fpw = fdopen(config->s, "rb+");

    // Determine TTY
    char ttypath[PATH_MAX];
    ssize_t ttypathlen = determine_tty(ttypath, sizeof ttypath);

    if (ttypathlen <= 0) {
        fprintf(stderr, "Failed to determine TTY\n");
        return 1;
    }
    //fprintf(stderr, "Detected TTY: %s\n", ttypath);

    // Remount everything as private
    mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL);

    // Preparations for pivot_root(2)
    char target[] = "/tmp/ispawn.XXXXXX";
    mkdtemp(target);
    fprintf(fpw, "%s\n", target);
    fflush(fpw);
    char put_old[] = "/tmp/ispawn.XXXXXX/mnt/oldroot";
    strncpy(put_old, target, strlen(target));
    // Must mount BEFORE creating put_old directory
    mount(config->path, target, NULL, MS_BIND | MS_PRIVATE, NULL);
    mkdir(put_old, 0755);

    // Mount necessary stuff
    mount("none", "proc", "proc", 0, NULL);
    mount("none", "sys", "sysfs", MS_RDONLY, NULL);
    mount("none", "tmp", "tmpfs", 0, NULL);
    mount("none", "dev", "tmpfs", MS_PRIVATE, NULL);

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

    // pivot_root(2)
    chdir(target);
    if (syscall(SYS_pivot_root, ".", "mnt/oldroot") == -1) {
        perror("pivot_root");
        return 1;
    }
    chdir("/");
    if (umount2("/mnt/oldroot", MNT_DETACH) == -1) {
        perror("Failed to umount old root");
        return -1;
    }
    if (rmdir("/mnt/oldroot") == -1)
        return -1;

    // Drop capabilities
    drop_caps();

    // Swap std{in,out,err}
    /*
    int fd;
    fd = open("/dev/console", O_RDONLY);
    dup2(fd, STDIN_FILENO);
    close(fd);
    fd = open("/dev/console", O_WRONLY);
    dup2(fd, STDOUT_FILENO);
    close(fd);
    fd = open("/dev/console", O_RDWR);
    dup2(fd, STDERR_FILENO);
    close(fd);
    */

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
        .path = argv[1],
        .argv = argv + 2
    };
    {
        int sv[2];
        socketpair(AF_LOCAL, SOCK_STREAM, 0, sv);
        config.s = sv[0];
        config.sparent = sv[1];
    }
    pid_t pid = clone((int(*)(void*))child, child_stack_start,
                      SIGCHLD | // Without SIGCHLD in flags we can't wait(2) for it
                      CLONE_NEWCGROUP | CLONE_NEWIPC | CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS,
                      &config);
    close(config.s);
    if (pid == -1) {
        perror("clone");
        return 1;
    }

    // Read the temp directory from child
    FILE *fpchild = fdopen(config.sparent, "rb+");
    char tempdir[PATH_MAX] = {};
    fgets(tempdir, sizeof tempdir, fpchild);
    if (tempdir[strlen(tempdir) - 1] = '\n')
        tempdir[strlen(tempdir) - 1] = 0;

    // Parent waits for child
    int status, ecode;
    wait(&status);
    if (WIFEXITED(status)) {
        printf("Exited with status %d\n", WEXITSTATUS(status));
        ecode = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        printf("Killed by signal %d\n", WTERMSIG(status));
        ecode = -WTERMSIG(status);
    }

    // Cleanup
    rmdir(tempdir);
    return ecode;
}
