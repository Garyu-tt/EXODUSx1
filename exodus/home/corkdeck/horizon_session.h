/* Managed application session for a permitted SystemApplet. Never elevates. */
#ifndef EXODUS_HORIZON_SESSION_H
#define EXODUS_HORIZON_SESSION_H
#include "horizon_observer.h"
#include <stddef.h>

enum exodus_session_state { EXODUS_SESSION_IDLE, EXODUS_SESSION_ACCEPTED,
    EXODUS_SESSION_RUNNING, EXODUS_SESSION_EXITED, EXODUS_SESSION_FAILED,
    EXODUS_SESSION_UNOBSERVED_EXIT, EXODUS_SESSION_UNKNOWN };
struct exodus_session {
    uint64_t title;
    struct exodus_horizon_sample baseline;
    enum exodus_session_state state;
    unsigned exit_reason;
};
/* A successful start is acceptance only. No implicit running/exit event. */
static inline int exodus_session_begin(struct exodus_session *s, uint64_t title,
                                      struct exodus_horizon_sample before)
{
    if (!s || !title || s->state!=EXODUS_SESSION_IDLE || before.program_id==title) return 0;
    s->title=title; s->baseline=before; s->state=EXODUS_SESSION_ACCEPTED; return 1;
}
static inline enum exodus_session_state exodus_session_sample(struct exodus_session *s,
                                                               struct exodus_horizon_sample sample)
{
    if (s->state==EXODUS_SESSION_ACCEPTED &&
        exodus_horizon_observe(s->title,s->baseline,sample)==EXODUS_OBSERVATION_TARGET_PRESENT)
        s->state=EXODUS_SESSION_RUNNING;
    return s->state;
}
/* Only an authoritative application-accessor finish event calls this function.
 * Raw Horizon exit reason is not a POSIX exit code. */
static inline enum exodus_session_state exodus_session_finish(struct exodus_session *s,unsigned reason)
{
    if (s->state==EXODUS_SESSION_ACCEPTED || s->state==EXODUS_SESSION_RUNNING) {
        s->exit_reason=reason;
        s->state=s->state==EXODUS_SESSION_RUNNING ? EXODUS_SESSION_EXITED : EXODUS_SESSION_UNOBSERVED_EXIT;
    }
    return s->state;
}
#ifdef __SWITCH__
struct exodus_managed_session {
    struct exodus_session lifecycle;
    AppletApplication application;
    int active;
};
/* The caller must verify installed title and obtain user confirmation first.
 * This API is intentionally not enabled by default in the NRO UI. */
static inline int exodus_managed_start(struct exodus_managed_session *s,uint64_t title,
                                       int verified,int confirmed)
{
    if (!s || s->active || s->lifecycle.state!=EXODUS_SESSION_IDLE || !title ||
        !verified || !confirmed || appletGetAppletType()!=AppletType_SystemApplet) return 0;
    const struct exodus_horizon_sample before=exodus_horizon_read_sample();
    if (before.program_id==title) return 0;
    if (R_FAILED(appletCreateApplication(&s->application,title))) {
        s->lifecycle.state=EXODUS_SESSION_FAILED; return 0;
    }
    s->active=1;
    if (R_FAILED(appletApplicationStart(&s->application))) {
        appletApplicationClose(&s->application); s->active=0;
        s->lifecycle.state=EXODUS_SESSION_FAILED; return 0;
    }
    return exodus_session_begin(&s->lifecycle,title,before);
}
static inline enum exodus_session_state exodus_managed_poll(struct exodus_managed_session *s)
{
    if (!s->active) return s->lifecycle.state;
    if (appletApplicationCheckFinished(&s->application)) {
        appletApplicationJoin(&s->application);
        unsigned reason=(unsigned)appletApplicationGetExitReason(&s->application);
        appletApplicationClose(&s->application); s->active=0;
        return exodus_session_finish(&s->lifecycle,reason);
    }
    return exodus_session_sample(&s->lifecycle,exodus_horizon_read_sample());
}
/* Closing the local accessor is not a termination request or exit evidence. */
static inline void exodus_managed_detach(struct exodus_managed_session *s)
{
    if (s->active) {
        appletApplicationClose(&s->application); s->active=0;
        s->lifecycle.state=EXODUS_SESSION_UNKNOWN;
    }
}
#endif
#endif
