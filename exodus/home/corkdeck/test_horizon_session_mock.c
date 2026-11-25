#include "horizon_session.h"
#include <assert.h>
#include <stdio.h>
static int create_error,start_error,finished,closes,creates,starts,joins;
static unsigned reason;
static AppletType role=AppletType_SystemApplet;
static u64 pid=10,program=1;
Result pmdmntInitialize(void){return 0;}
Result pminfoInitialize(void){return 0;}
Result pmdmntGetApplicationProcessId(u64 *out){*out=pid;return 0;}
Result pminfoGetProgramId(u64 *out,u64 p){assert(p==pid);*out=program;return 0;}
void pmdmntExit(void){}
void pminfoExit(void){}
AppletType appletGetAppletType(void){return role;}
Result appletCreateApplication(AppletApplication *a,u64 id){assert(id==2);++creates;if(create_error)return 1;a->initialized=1;return 0;}
Result appletApplicationStart(AppletApplication *a){assert(a->initialized);++starts;return start_error;}
int appletApplicationCheckFinished(AppletApplication *a){assert(a->initialized);return finished;}
void appletApplicationJoin(AppletApplication *a){assert(a->initialized && finished);++joins;}
unsigned appletApplicationGetExitReason(AppletApplication *a){assert(a->initialized && joins);return reason;}
void appletApplicationClose(AppletApplication *a){assert(a->initialized);a->initialized=0;++closes;}
int main(void)
{
    struct exodus_managed_session s={0};
    role=AppletType_Application;
    assert(!exodus_managed_start(&s,2,1,1));
    role=AppletType_SystemApplet;
    assert(!exodus_managed_start(&s,2,0,1));
    assert(!exodus_managed_start(&s,2,1,0));
    assert(!creates);
    program=2;assert(!exodus_managed_start(&s,2,1,1));program=1;
    create_error=1;assert(!exodus_managed_start(&s,2,1,1));assert(!closes && !starts);
    s=(struct exodus_managed_session){0};create_error=0;start_error=1;
    assert(!exodus_managed_start(&s,2,1,1));assert(closes==1 && !s.active);
    s=(struct exodus_managed_session){0};start_error=0;
    assert(exodus_managed_start(&s,2,1,1));assert(!exodus_managed_start(&s,2,1,1));
    assert(exodus_managed_poll(&s)==EXODUS_SESSION_ACCEPTED);
    pid=11;program=2;assert(exodus_managed_poll(&s)==EXODUS_SESSION_RUNNING);
    pid=0;program=0;assert(exodus_managed_poll(&s)==EXODUS_SESSION_RUNNING);
    finished=1;reason=4;assert(exodus_managed_poll(&s)==EXODUS_SESSION_EXITED);
    assert(s.lifecycle.exit_reason==4 && closes==2 && joins==1);
    assert(exodus_managed_poll(&s)==EXODUS_SESSION_EXITED);assert(closes==2);
    s=(struct exodus_managed_session){0};pid=10;program=1;finished=0;
    assert(exodus_managed_start(&s,2,1,1));finished=1;reason=0;
    assert(exodus_managed_poll(&s)==EXODUS_SESSION_UNOBSERVED_EXIT);
    s=(struct exodus_managed_session){0};finished=0;
    assert(exodus_managed_start(&s,2,1,1));exodus_managed_detach(&s);exodus_managed_detach(&s);
    assert(s.lifecycle.state==EXODUS_SESSION_UNKNOWN && closes==4 && joins==2);
    puts("PASS: managed session gates, start failure, lifecycle and cleanup");
    return 0;
}
