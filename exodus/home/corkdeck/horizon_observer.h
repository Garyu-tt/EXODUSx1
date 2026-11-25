/* Read-only application-process evidence; no debug attach or launch. */
#ifndef EXODUS_HORIZON_OBSERVER_H
#define EXODUS_HORIZON_OBSERVER_H
#include <stdint.h>
struct exodus_horizon_sample { uint64_t pid, program_id; };
enum exodus_observation { EXODUS_OBSERVATION_UNKNOWN, EXODUS_OBSERVATION_OTHER,
    EXODUS_OBSERVATION_TARGET_PRESENT, EXODUS_OBSERVATION_TARGET_PREEXISTING };
/* Caller must independently establish launch acceptance. Presence is not
 * proof of execution or exit; missing/denied services remain unknown. */
static inline enum exodus_observation exodus_horizon_observe(
    uint64_t title, struct exodus_horizon_sample before,
    struct exodus_horizon_sample after)
{
    if (!title || !after.pid || !after.program_id) return EXODUS_OBSERVATION_UNKNOWN;
    if (after.program_id != title) return EXODUS_OBSERVATION_OTHER;
    if (!before.pid || !before.program_id) return EXODUS_OBSERVATION_UNKNOWN;
    if (before.pid == after.pid)
        return before.program_id == title ? EXODUS_OBSERVATION_TARGET_PREEXISTING : EXODUS_OBSERVATION_UNKNOWN;
    return EXODUS_OBSERVATION_TARGET_PRESENT;
}
#ifdef __SWITCH__
#include <switch.h>
/* Uses permissions already granted to the caller, never modifies them.
 * Double-read PID to reject an application switch during the sample. */
static inline struct exodus_horizon_sample exodus_horizon_read_sample(void)
{
    struct exodus_horizon_sample result={0,0};
    u64 first=0,last=0,program=0;
    if (R_FAILED(pmdmntInitialize())) return result;
    if (R_SUCCEEDED(pminfoInitialize())) {
        if (R_SUCCEEDED(pmdmntGetApplicationProcessId(&first)) && first &&
            R_SUCCEEDED(pminfoGetProgramId(&program,first)) && program &&
            R_SUCCEEDED(pmdmntGetApplicationProcessId(&last)) && first==last) {
            result.pid=first; result.program_id=program;
        }
        pminfoExit();
    }
    pmdmntExit();
    return result;
}
#else
static inline struct exodus_horizon_sample exodus_horizon_read_sample(void)
{
    struct exodus_horizon_sample result={0,0}; return result;
}
#endif
#endif
