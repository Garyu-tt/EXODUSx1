#include "horizon_observer.h"
#include <assert.h>
#include <stdio.h>
static int fail_step, step, dmnt_closed, info_closed, changed;
static int advance(void) { return ++step==fail_step; }
Result pmdmntInitialize(void) { return advance(); }
Result pminfoInitialize(void) { return advance(); }
Result pmdmntGetApplicationProcessId(u64 *out) { *out=(step==4 && changed)?11:10; return advance(); }
Result pminfoGetProgramId(u64 *out,u64 pid) { assert(pid==10); *out=2; return advance(); }
void pmdmntExit(void) { ++dmnt_closed; }
void pminfoExit(void) { ++info_closed; }
int main(void)
{
    for(fail_step=0;fail_step<=5;++fail_step) {
        step=dmnt_closed=info_closed=0;
        struct exodus_horizon_sample sample=exodus_horizon_read_sample();
        assert(sample.pid==(fail_step?0:10));
        assert(sample.program_id==(fail_step?0:2));
        assert(dmnt_closed==(fail_step!=1));
        assert(info_closed==(fail_step!=1 && fail_step!=2));
    }
    fail_step=step=dmnt_closed=info_closed=0; changed=1;
    assert(!exodus_horizon_read_sample().pid);
    assert(dmnt_closed==1 && info_closed==1);
    puts("PASS: simulated PM errors, cleanup, PID switch race");
    return 0;
}
