#ifndef EXODUS_CAPABILITY_POLICY_H
#define EXODUS_CAPABILITY_POLICY_H

/* Reference policy for EXODUS-owned dispatch. Not a Horizon sandbox.
 * Callers must classify trusted operation IDs, not accept IDs from plugins.
 * Unknown operations are denied. Do not use a generic allowed ID for a
 * more privileged operation. Network destinations and paths require
 * separate authorization by their owning providers.
 */
typedef enum {
    EX_CAP_UNKNOWN = 0,
    EX_CAP_READ_LOCAL_STATUS = 1,
    EX_CAP_READ_LOCAL_CATALOG = 2,
    EX_CAP_READ_LOCAL_FILE = 3,
    EX_CAP_SSH_CLIENT = 4,
    EX_CAP_LAN_SERVICE = 5,
    EX_CAP_HOMEBREW_LAUNCH = 6,
    EX_CAP_NATIVE_TITLE_LAUNCH = 7,
    EX_CAP_NINTENDO_ESHOP = 20,
    EX_CAP_NINTENDO_ACCOUNT = 21,
    EX_CAP_NINTENDO_ONLINE = 22,
    EX_CAP_NINTENDO_SERVICE = 23,
    EX_CAP_LICENSE_MUTATION = 24,
    EX_CAP_SYSTEM_CONTENT_INSTALL = 25,
    EX_CAP_UNREVIEWED_PLUGIN = 26,
    EX_CAP_RAW_PRIVILEGED_DISPATCH = 27
} ex_capability;

typedef enum {
    EX_CAP_DENIED = 0,
    EX_CAP_ALLOWED = 1,
    EX_CAP_REQUIRES_PROVIDER_REVIEW = 2
} ex_cap_decision;

static inline ex_cap_decision ex_cap_classify(ex_capability cap)
{
    switch (cap) {
    case EX_CAP_READ_LOCAL_STATUS:
    case EX_CAP_READ_LOCAL_CATALOG:
        return EX_CAP_ALLOWED;
    case EX_CAP_READ_LOCAL_FILE:
    case EX_CAP_SSH_CLIENT:
    case EX_CAP_LAN_SERVICE:
    case EX_CAP_HOMEBREW_LAUNCH:
    case EX_CAP_NATIVE_TITLE_LAUNCH:
        return EX_CAP_REQUIRES_PROVIDER_REVIEW;
    default:
        return EX_CAP_DENIED;
    }
}

/* No caller-controlled "approved" flag: reviewed capabilities remain
 * unavailable until a trusted provider policy is implemented and tested. */
static inline int ex_cap_may_dispatch(ex_capability cap)
{
    return ex_cap_classify(cap) == EX_CAP_ALLOWED;
}

static inline int ex_cap_may_discover(ex_capability cap)
{
    return ex_cap_may_dispatch(cap);
}
#endif
