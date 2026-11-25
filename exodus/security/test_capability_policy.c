#include "capability_policy.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    int i;
    for (i = -100; i <= 100; ++i) {
        ex_capability cap = (ex_capability)i;
        int allowed = i == EX_CAP_READ_LOCAL_STATUS ||
                      i == EX_CAP_READ_LOCAL_CATALOG;
        assert(ex_cap_may_dispatch(cap) == allowed);
        assert(ex_cap_may_discover(cap) == allowed);
    }
    assert(ex_cap_classify(EX_CAP_SSH_CLIENT) == EX_CAP_REQUIRES_PROVIDER_REVIEW);
    assert(ex_cap_classify(EX_CAP_HOMEBREW_LAUNCH) == EX_CAP_REQUIRES_PROVIDER_REVIEW);
    assert(ex_cap_classify(EX_CAP_NATIVE_TITLE_LAUNCH) == EX_CAP_REQUIRES_PROVIDER_REVIEW);
    assert(ex_cap_classify(EX_CAP_NINTENDO_ESHOP) == EX_CAP_DENIED);
    assert(ex_cap_classify(EX_CAP_NINTENDO_ACCOUNT) == EX_CAP_DENIED);
    assert(ex_cap_classify(EX_CAP_NINTENDO_ONLINE) == EX_CAP_DENIED);
    puts("capability policy: 408 assertions passed");
    return 0;
}
