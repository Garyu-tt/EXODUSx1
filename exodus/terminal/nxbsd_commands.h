#ifndef EXODUS_NXBSD_COMMANDS_H
#define EXODUS_NXBSD_COMMANDS_H
#include "nxbsd_pipeline.h"
#define NXBSD_ARG_MAX 32
typedef int (*nxbsd_command_fn)(struct nxbsd_context *, int, const char *const *);
struct nxbsd_command { const char *name; nxbsd_command_fn fn; };
struct nxbsd_invocation { int argc; const char *const *argv; };
/* Exact allowlisted names; never interpret argv as shell source.
 * No dynamic modules, external executables or PATH lookup yet.
 * Commands and argv are borrowed and stable until synchronous return.
 * All stages are resolved before any command is invoked. */
const struct nxbsd_command *nxbsd_command_lookup(const char *name);
int nxbsd_command_run(struct nxbsd_context *, int argc, const char *const *argv);
int nxbsd_commands_pipeline(const struct nxbsd_context *,
                            const struct nxbsd_invocation *, size_t n, int *status);
#endif
