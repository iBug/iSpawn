#include "cap.h"

#include <stdio.h>
#include <sys/capability.h>
#include <sys/prctl.h>

static int caps_list[] = {
    CAP_AUDIT_CONTROL,
    CAP_AUDIT_READ,
    CAP_AUDIT_WRITE,
    CAP_BLOCK_SUSPEND,
    CAP_DAC_READ_SEARCH,
    CAP_FSETID,
    CAP_IPC_LOCK,
    CAP_MAC_ADMIN,
    CAP_MAC_OVERRIDE,
    CAP_MKNOD,
    CAP_SETFCAP,
    CAP_SYSLOG,
    //CAP_SYS_ADMIN,
    CAP_SYS_BOOT,
    CAP_SYS_MODULE,
    CAP_SYS_NICE,
    CAP_SYS_RAWIO,
    CAP_SYS_RESOURCE,
    CAP_SYS_TIME,
    CAP_WAKE_ALARM
};
size_t num_caps = sizeof(caps_list) / sizeof(*caps_list);

int drop_caps(void) {
    // Drop capabilities
    for (size_t i = 0; i < num_caps; i++) {
        if (prctl(PR_CAPBSET_DROP, caps_list[i], 0, 0, 0) == -1) {
            perror("prctl");
            return -1;
        }
    }

    cap_t caps = cap_get_proc();
    if (!caps) {
        perror("cap_get_proc");
        return -1;
    }

    if (cap_set_flag(caps, CAP_INHERITABLE, num_caps, caps_list, CAP_CLEAR) == -1 ||
            cap_set_proc(caps) == -1) {
        perror("cap_set_proc");
        cap_free(caps);
        return -1;
    }
    cap_free(caps);
    return 0;
}
