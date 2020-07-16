#include "cap.h"

#include <cap-ng.h>

int drop_caps(void) {
    capng_clear(CAPNG_SELECT_BOTH);
    capng_updatev(CAPNG_ADD, (capng_type_t)(CAPNG_EFFECTIVE | CAPNG_PERMITTED | CAPNG_BOUNDING_SET),
        CAP_SETPCAP,
        CAP_MKNOD,
        CAP_AUDIT_WRITE,
        CAP_CHOWN,
        CAP_NET_RAW,
        CAP_DAC_OVERRIDE,
        CAP_FOWNER,
        CAP_FSETID,
        CAP_KILL,
        CAP_SETGID,
        CAP_SETUID,
        CAP_NET_BIND_SERVICE,
        CAP_SYS_CHROOT,
        CAP_SETFCAP,
        -1);
    capng_apply(CAPNG_SELECT_BOTH);
    return 0;
}
