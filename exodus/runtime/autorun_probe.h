#ifndef EXODUS_AUTORUN_PROBE_H
#define EXODUS_AUTORUN_PROBE_H

#define EXODUS_AUTORUN_PATH "sdmc:/switch/wine/wine-nx-runtime.nro"

enum exodus_autorun_status {
    EXODUS_AUTORUN_READY,
    EXODUS_AUTORUN_MISSING,
    EXODUS_AUTORUN_UNREADABLE,
    EXODUS_AUTORUN_INVALID_NRO,
};

enum exodus_autorun_status exodus_autorun_probe(const char *path);

#endif
