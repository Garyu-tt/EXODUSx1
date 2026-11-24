#ifndef EXODUS_WINDOWS_PE_H
#define EXODUS_WINDOWS_PE_H

#include <stdbool.h>
#include <stdio.h>

/* Check the file ranges of a PE32 or PE32+ application image on a seekable binary stream. */
bool exodus_windows_pe_valid(FILE *image);

#endif
