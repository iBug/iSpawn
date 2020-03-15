#include <stdio.h>
#include <unistd.h>

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
    execvp(argv[2], argv + 2);
    perror("exec");
    return 1;
}
