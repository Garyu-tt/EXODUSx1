// SPDX-License-Identifier: GPL-2.0-only
#include "shell_menu.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    assert(exodus_page_move(PAGE_LIBRARY, -1) == PAGE_EXIT);
    assert(exodus_page_move(PAGE_EXIT, 1) == PAGE_LIBRARY);
    assert(exodus_page_move(PAGE_SETTINGS, 1) == PAGE_TERMINAL);
    assert(exodus_page_move(PAGE_TERMINAL, 1) == PAGE_SSH);
    assert(exodus_page_move(PAGE_SSH, -1) == PAGE_TERMINAL);
    assert(exodus_page_move(PAGE_SSH, 0) == PAGE_SSH);
    assert(exodus_page_move(PAGE_COUNT, 1) == PAGE_LIBRARY);
    assert(!exodus_pages[PAGE_LIBRARY].nro && !exodus_pages[PAGE_SETTINGS].nro);
    assert(!strcmp(exodus_pages[PAGE_TERMINAL].nro, "sdmc:/switch/exodus-terminal/exodus-terminal.nro"));
    assert(!strcmp(exodus_pages[PAGE_SSH].nro, "sdmc:/switch/exodus-ssh/exodus-ssh.nro"));
    assert(!exodus_pages[PAGE_EXIT].nro);
    return 0;
}
