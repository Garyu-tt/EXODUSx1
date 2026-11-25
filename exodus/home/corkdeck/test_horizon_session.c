#include "horizon_session.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    const struct exodus_horizon_sample before={10,1}, after={11,2}, missing={0,0};
    struct exodus_session s={0};
    assert(!exodus_session_begin(&s,1,before));
    assert(exodus_session_begin(&s,2,before));
    assert(!exodus_session_begin(&s,3,before));
    assert(exodus_session_sample(&s,missing)==EXODUS_SESSION_ACCEPTED);
    assert(exodus_session_sample(&s,after)==EXODUS_SESSION_RUNNING);
    assert(exodus_session_sample(&s,missing)==EXODUS_SESSION_RUNNING);
    assert(exodus_session_finish(&s,4)==EXODUS_SESSION_EXITED && s.exit_reason==4);
    assert(exodus_session_sample(&s,after)==EXODUS_SESSION_EXITED);
    assert(exodus_session_finish(&s,0)==EXODUS_SESSION_EXITED && s.exit_reason==4);
    s=(struct exodus_session){0};
    assert(exodus_session_begin(&s,2,missing));
    assert(exodus_session_sample(&s,after)==EXODUS_SESSION_ACCEPTED);
    assert(exodus_session_finish(&s,0)==EXODUS_SESSION_UNOBSERVED_EXIT);
    puts("PASS: authoritative session evidence and terminal state boundaries");
    return 0;
}
