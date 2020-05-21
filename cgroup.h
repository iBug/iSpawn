#ifndef __CGROUP_H
#define __CGROUP_H

#include <sys/types.h>

int mount_cgroup(void);
int set_cgroup(pid_t);
int reset_cgroup(void);

#endif
