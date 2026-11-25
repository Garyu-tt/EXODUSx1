/* Test-only ABI shim, never part of a native build. */
#ifndef EXODUS_TEST_SWITCH_H
#define EXODUS_TEST_SWITCH_H
#include <stdint.h>
typedef uint64_t u64;
typedef int Result;
#define R_FAILED(rc) ((rc)!=0)
#define R_SUCCEEDED(rc) ((rc)==0)
Result pmdmntInitialize(void);
Result pminfoInitialize(void);
Result pmdmntGetApplicationProcessId(u64 *out);
Result pminfoGetProgramId(u64 *out,u64 pid);
void pmdmntExit(void);
void pminfoExit(void);
typedef enum { AppletType_Application, AppletType_SystemApplet } AppletType;
typedef struct { int initialized; } AppletApplication;
AppletType appletGetAppletType(void);
Result appletCreateApplication(AppletApplication *a,u64 id);
Result appletApplicationStart(AppletApplication *a);
int appletApplicationCheckFinished(AppletApplication *a);
void appletApplicationJoin(AppletApplication *a);
unsigned appletApplicationGetExitReason(AppletApplication *a);
void appletApplicationClose(AppletApplication *a);
#endif
