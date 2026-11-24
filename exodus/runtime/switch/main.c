#include "autorun_probe.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <switch.h>

static void show_runtime(enum exodus_autorun_status status, bool application_mode,
                         bool next_load, Result launch_result)
{
    consoleClear();
    puts("EXODUS Windows Runtime\n");
    switch (status) {
    case EXODUS_AUTORUN_READY:
        puts("Autorun launcher found.");
        break;
    case EXODUS_AUTORUN_MISSING:
        puts("Autorun launcher is missing.");
        break;
    case EXODUS_AUTORUN_UNREADABLE:
        puts("Could not read the Autorun launcher.");
        break;
    case EXODUS_AUTORUN_INVALID_NRO:
        puts("The Autorun launcher has an invalid or incomplete NRO image.");
        break;
    }
    puts(EXODUS_AUTORUN_PATH);
    if (!application_mode)
        puts("\nOpen the Homebrew Menu in application mode to launch Autorun.");
    if (!next_load)
        puts("\nThe current loader does not support a homebrew handoff.");
    if (R_FAILED(launch_result))
        printf("\nCould not schedule Autorun (0x%08" PRIx32 ").\n", launch_result);
    if (status == EXODUS_AUTORUN_READY && application_mode && next_load)
        puts("\nA  Open Autorun");
    puts("X  Rescan     B / +  Exit");
}

int main(void)
{
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    const bool application_mode = appletGetAppletType() == AppletType_Application;
    const bool next_load = envHasNextLoad();
    enum exodus_autorun_status status = exodus_autorun_probe(EXODUS_AUTORUN_PATH);
    show_runtime(status, application_mode, next_load, 0);

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 buttons = padGetButtonsDown(&pad);
        if (buttons & (HidNpadButton_B | HidNpadButton_Plus))
            break;
        if (buttons & HidNpadButton_X) {
            status = exodus_autorun_probe(EXODUS_AUTORUN_PATH);
            show_runtime(status, application_mode, next_load, 0);
        } else if ((buttons & HidNpadButton_A) && application_mode && next_load) {
            status = exodus_autorun_probe(EXODUS_AUTORUN_PATH);
            Result launch_result = 0;
            if (status == EXODUS_AUTORUN_READY) {
                launch_result = envSetNextLoad(EXODUS_AUTORUN_PATH, EXODUS_AUTORUN_PATH);
                if (R_SUCCEEDED(launch_result))
                    break;
            }
            show_runtime(status, application_mode, next_load, launch_result);
        }
        consoleUpdate(NULL);
    }

    consoleExit(NULL);
    return EXIT_SUCCESS;
}
