#include "syscall.h"

#include <stdio.h>
#include <errno.h>

#include "syscall_allow.h"

int filter_syscall(void) {
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ERRNO(1));
    if (ctx == NULL) {
        return -1;
    }
    for (int i = 0; i < allowed_syscalls_len; i++) {
        if ((errno = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, allowed_syscalls[i], 0)) < 0) {
            return -1;
        }
    }
    if ((errno = -seccomp_load(ctx)) < 0) {
        return -1;
    }
    seccomp_release(ctx);
    return 0;
}
