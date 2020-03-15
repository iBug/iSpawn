#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/wait.h>

const char *usage =
"Usage: %s <directory> <command> [args...]\n"
"\n"
"  Run <directory> as a container and execute <command>.\n";

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, usage, argv[0]);
        return 1;
    }
    if (chdir(argv[1]) == -1) {
        perror(argv[1]);
        return 1;
    }
    if (chroot(".") == -1) {
        perror("chroot");
        return 1;
    }
    if (unshare(CLONE_NEWCGROUP | CLONE_NEWIPC | CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWPID) == -1) {
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
        execvp(argv[2], argv + 2);
        perror("exec");
        return 1;
    }
    // Parent does nothing
    int status;
    wait(&status);
    if (WIFEXITED(status)) {
        printf("Exited with status %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("Killed by signal %d\n", WTERMSIG(status));
    }
    return 0;
}
