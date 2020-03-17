#ifndef __SYSCALL_ALLOW_H
#define __SYSCALL_ALLOW_H

#include <seccomp.h>
#include <sys/types.h>

extern int allowed_syscalls[];
extern size_t allowed_syscalls_len;

#endif
