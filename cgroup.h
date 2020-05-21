#ifndef __CGROUP_H
#define __CGROUP_H

#include <sys/types.h>

int save_cgroup(pid_t);
int restore_cgroup(pid_t);
int mount_cgroup(void);
int apply_cgroup(pid_t);
int create_cgroup(void);
int set_cgroup(void);
int reset_cgroup(void);

#endif
