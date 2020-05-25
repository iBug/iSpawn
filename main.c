#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> // For PATH_MAX
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>

#include "cap.h"
#include "cgroup.h"
#include "fs.h"
#include "syscall.h"
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
    int s, sparent; // Socket communication with parent
} SpawnConfig;

int child(SpawnConfig *config) {
    char **argv = config->argv;
    if (config->sparent)
        close(config->sparent);
    FILE *fpw = fdopen(config->s, "rb+");
    char target[PATH_MAX];
    prepare_fs(".", target);
    sethostname("iSpawn", 6);

    // pivot_root(2)
    chdir(target);
    if (syscall(SYS_pivot_root, ".", "oldroot") == -1) {
        perror("pivot_root");
        return 1;
    }
    chdir("/");
    if (umount2("/oldroot", MNT_DETACH) == -1) {
        perror("Failed to umount old root");
        //return -1; // Non-critical
    } else if (rmdir("/oldroot") == -1) {
        perror("Failed to remove old root directory");
        //return -1;
    }

    // Ready to inform parent to hide mount point
    fprintf(fpw, "%s\n", target);
    fflush(fpw);

    // Wait for parent to complete cgroup setup
    read(config->s, target, 1);
    unshare(CLONE_NEWCGROUP);
    // Setup cgroup
    mount_cgroup();

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

    // Apply seccomp rules
    filter_syscall();

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
    const size_t stack_size = 1024 * 1024;
    void *child_stack = mmap(NULL, stack_size,
                             PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                             -1, 0);
    void *child_stack_start = child_stack + stack_size;

    SpawnConfig config = {
        .argv = argv + 2
    };
    {
        int sv[2];
        socketpair(AF_LOCAL, SOCK_STREAM, 0, sv);
        config.s = sv[0];
        config.sparent = sv[1];
    }
    pid_t pid = clone((int(*)(void*))child, child_stack_start,
                      // NEWCGROUP done by unshare(2) in child
                      CLONE_NEWIPC | CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS |
                      SIGCHLD, // Without SIGCHLD in flags we can't wait(2) for it
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
    if (tempdir[strlen(tempdir) - 1] == '\n')
        tempdir[strlen(tempdir) - 1] = 0;

    // Lazy umount the temp dir so container isn't visible from that path
    umount2(tempdir, MNT_DETACH);
    rmdir(tempdir);

    // Move child to cgroup
    set_cgroup(pid);
    // Notify child
    write(config.sparent, "", 1);

    // Parent waits for child
    int status, ecode;
    wait(&status);
    if (WIFEXITED(status)) {
        printf("iSpawn exited with status %d\n", WEXITSTATUS(status));
        ecode = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        printf("iSpawn killed by signal %d\n", WTERMSIG(status));
        ecode = 0x80 | WTERMSIG(status);
    }

    // Cleanup cgroup
    reset_cgroup();

    return ecode;
}
