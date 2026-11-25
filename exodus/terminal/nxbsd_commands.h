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
#define NXBSD_REDIR_MAX 8
enum nxbsd_redir_kind { NXBSD_REDIR_IN, NXBSD_REDIR_OUT, NXBSD_REDIR_APPEND,
                       NXBSD_REDIR_EXCLUSIVE, NXBSD_REDIR_DUP, NXBSD_REDIR_CLOSE };
struct nxbsd_redir { enum nxbsd_redir_kind kind; int fd, source; const char *path; };
struct nxbsd_redir_list { size_t count; struct nxbsd_redir items[NXBSD_REDIR_MAX]; };
/* Apply in source order after pipeline wiring. File effects are not rollbackable.
 * On redirection error the affected stage returns 1 without invoking its command. */
int nxbsd_commands_pipeline_redirect(const struct nxbsd_context *,
    const struct nxbsd_invocation *, const struct nxbsd_redir_list *, size_t n, int *status);
#endif
