/* SPDX-License-Identifier: GPL-2.0-only
 * Logical game graph. Inputs are explicit owner manifest or verified metadata,
 * never inferred from filenames. This graph grants no launch authorization. */
#ifndef EXODUS_EVCS_GRAPH_H
#define EXODUS_EVCS_GRAPH_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define EVCS_GRAPH_LIMIT 4096u
enum evcs_role { EVCS_BASE, EVCS_UPDATE, EVCS_DLC };
enum evcs_graph_result { EVCS_GRAPH_OK, EVCS_GRAPH_INVALID, EVCS_GRAPH_LIMIT_EXCEEDED, EVCS_GRAPH_DUPLICATE };
struct evcs_content {
    uint64_t application_id; /* explicit base application identity */
    uint64_t content_id;     /* explicit title identity, not an NCA hash */
    uint32_t version;
    enum evcs_role role;
};
struct evcs_group {
    size_t base, update; /* SIZE_MAX means no candidate */
    size_t dlc_count;
};
/* All input records are validated before resolving groups. Distinct versions
 * may coexist, but the same title/version cannot claim different roles or bases. */
static inline enum evcs_graph_result evcs_graph_validate(const struct evcs_content *items, size_t n)
{
    if (n > EVCS_GRAPH_LIMIT) return EVCS_GRAPH_LIMIT_EXCEEDED;
    if (n && !items) return EVCS_GRAPH_INVALID;
    for (size_t i=0; i<n; ++i) {
        const struct evcs_content *a=&items[i];
        if (!a->application_id || !a->content_id ||
            (a->role!=EVCS_BASE && a->role!=EVCS_UPDATE && a->role!=EVCS_DLC) ||
            ((a->role==EVCS_BASE) != (a->application_id==a->content_id)))
            return EVCS_GRAPH_INVALID;
        for (size_t j=0; j<i; ++j) {
            const struct evcs_content *b=&items[j];
            if (a->content_id!=b->content_id) continue;
            if (a->application_id!=b->application_id || a->role!=b->role)
                return EVCS_GRAPH_INVALID;
            if (a->version==b->version) return EVCS_GRAPH_DUPLICATE;
        }
    }
    return EVCS_GRAPH_OK;
}
/* Call only with validated records. Returns logical candidates, not an
 * installation plan: update and DLC compatibility still require native checks. */
static inline struct evcs_group evcs_graph_group(const struct evcs_content *items, size_t n, uint64_t app)
{
    struct evcs_group g={SIZE_MAX,SIZE_MAX,0};
    if (!items || !app || n>EVCS_GRAPH_LIMIT) return g;
    for (size_t i=0; i<n; ++i) {
        const struct evcs_content *a=&items[i];
        if (a->application_id!=app) continue;
        if (a->role==EVCS_BASE) {
            if (g.base==SIZE_MAX || a->version>items[g.base].version) g.base=i;
        } else if (a->role==EVCS_UPDATE) {
            if (g.update==SIZE_MAX || a->version>items[g.update].version ||
                (a->version==items[g.update].version && a->content_id<items[g.update].content_id))
                g.update=i;
        } else if (a->role==EVCS_DLC) {
            int first=1;
            for (size_t j=0; j<i; ++j)
                if (items[j].application_id==app && items[j].role==EVCS_DLC &&
                    items[j].content_id==a->content_id) { first=0; break; }
            if (first) ++g.dlc_count;
        }
    }
    return g;
}
#endif
