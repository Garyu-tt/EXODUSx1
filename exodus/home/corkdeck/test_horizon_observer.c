#include "horizon_observer.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    const struct exodus_horizon_sample empty={0,0}, old={10,1}, next={11,2};
    const struct exodus_horizon_sample preexisting={10,2}, mismatched={10,3};
    assert(exodus_horizon_observe(2,old,next)==EXODUS_OBSERVATION_TARGET_PRESENT);
    assert(exodus_horizon_observe(2,old,empty)==EXODUS_OBSERVATION_UNKNOWN);
    assert(exodus_horizon_observe(2,empty,next)==EXODUS_OBSERVATION_UNKNOWN);
    assert(exodus_horizon_observe(2,preexisting,preexisting)==EXODUS_OBSERVATION_TARGET_PREEXISTING);
    assert(exodus_horizon_observe(2,mismatched,preexisting)==EXODUS_OBSERVATION_UNKNOWN);
    assert(exodus_horizon_observe(2,next,old)==EXODUS_OBSERVATION_OTHER);
    assert(exodus_horizon_observe(0,old,next)==EXODUS_OBSERVATION_UNKNOWN);
#ifndef __SWITCH__
    assert(!exodus_horizon_read_sample().pid);
#endif
    puts("PASS: process identity evidence, ambiguous and preexisting targets");
    return 0;
}
