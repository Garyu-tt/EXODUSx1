// SPDX-License-Identifier: GPL-2.0-only
/* Fixed local entry points; no remote Nintendo account/store actions. */
#ifndef EXODUS_SHELL_MENU_H
#define EXODUS_SHELL_MENU_H
#include <stddef.h>

enum exodus_page { PAGE_LIBRARY, PAGE_SETTINGS, PAGE_TERMINAL, PAGE_SSH, PAGE_RUNTIME, PAGE_EXIT, PAGE_COUNT };
struct exodus_page_info { const char *name, *description, *nro; };
static const struct exodus_page_info exodus_pages[PAGE_COUNT] = {
    {"Library", "Switch, Homebrew, Windows and Steam", NULL},
    {"Settings", "Read-only Horizon system information", NULL},
    {"Terminal", "EXODUS native terminal / BSD userland", "sdmc:/switch/exodus-terminal/exodus-terminal.nro"},
    {"SSH", "EXODUS native SSH client", "sdmc:/switch/exodus-ssh/exodus-ssh.nro"},
    {"Windows Runtime", "Open the installed CorkDeck launcher", NULL},
    {"Exit", "Return to the current loader", NULL}
};
static inline size_t exodus_page_move(size_t index, int delta)
{
    if (index >= PAGE_COUNT) return 0;
    if (delta < 0) return index ? index - 1 : PAGE_COUNT - 1;
    if (delta > 0) return (index + 1) % PAGE_COUNT;
    return index;
}
#endif
