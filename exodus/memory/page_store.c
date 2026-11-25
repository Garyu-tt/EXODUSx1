#include "page_store.h"

#include <string.h>


static bool basic(const struct exodus_page_store *s)
{
    return s && s->payload && s->page_ends &&
           s->page_count<=s->page_capacity && s->payload_used<=s->payload_capacity &&
           (!s->page_order||(s->page_starts&&s->page_capacity<=UINT32_MAX));
}

bool exodus_page_store_extent(const struct exodus_page_store *s,size_t i,
                              size_t *start,size_t *end)
{
    if(!basic(s)||!start||!end||i>=s->page_count)return false;
    size_t a=s->page_starts?s->page_starts[i]:(i?s->page_ends[i-1]:0);
    size_t b=s->page_ends[i];
    size_t limit=s->page_starts?s->payload_capacity:s->payload_used;
    if(a>=b||b>limit||b-a>EXODUS_PAGE_BYTES)return false;
    *start=a;*end=b;
    return true;
}

bool exodus_page_store_validate(const struct exodus_page_store *s)
{
    if(!basic(s))return false;
    size_t used=0;
    if(s->page_order){
        size_t last=0;
        for(size_t k=0;k<s->page_count;k++){
            size_t i=s->page_order[k],a,b;
            if(!exodus_page_store_extent(s,i,&a,&b)||a<last||
               b-a>s->payload_used-used)return false;
            last=b;used+=b-a;
        }
        /* Strict nonoverlap and nonempty extents imply distinct indices. */
        return used==s->payload_used;
    }
    for(size_t i=0;i<s->page_count;i++){
        size_t a,b;
        if(!exodus_page_store_extent(s,i,&a,&b)||b-a>s->payload_used-used)return false;
        used+=b-a;
        if(s->page_starts)for(size_t j=0;j<i;j++)
            if(a<s->page_ends[j]&&s->page_starts[j]<b)return false;
    }
    return used==s->payload_used;
}

/* Return the end of a conflicting extent, or the requested start if free.
 * The old extent may be reused by its replacement. All ends are validated. */
static size_t conflict(const struct exodus_page_store *s,size_t skip,size_t a,size_t n)
{
    size_t next=a;
    for(size_t i=0;i<s->page_count;i++){
        if(i==skip)continue;
        if(a<s->page_ends[i]&&s->page_starts[i]<a+n&&s->page_ends[i]>next)
            next=s->page_ends[i];
    }
    return next;
}


/* With sorted extents, first-fit needs one traversal, including old extent
 * reuse. No state changes until a destination has been found. */
static bool ordered_gap(const struct exodus_page_store *s,size_t skip,size_t n,size_t *out)
{
    size_t a=0;
    for(size_t k=0;k<s->page_count;k++){
        size_t i=s->page_order[k];
        if(i==skip)continue;
        if(s->page_starts[i]-a>=n){*out=a;return true;}
        a=s->page_ends[i];
    }
    if(s->payload_capacity-a<n)return false;
    *out=a;return true;
}
static void order_remove(struct exodus_page_store *s,size_t i)
{
    size_t k=0;
    while(k<s->page_count&&s->page_order[k]!=i)++k;
    /* Existing index was validated, so k is in range. */
    for(;k+1<s->page_count;k++)s->page_order[k]=s->page_order[k+1];
}
static void order_insert(struct exodus_page_store *s,size_t count,size_t i)
{
    size_t k=count;
    while(k&&s->page_starts[s->page_order[k-1]]>s->page_starts[i]){
        s->page_order[k]=s->page_order[k-1];--k;
    }
    s->page_order[k]=(uint32_t)i;
}
bool exodus_page_store_pool_stats(const struct exodus_page_store *s,
                                  struct exodus_page_pool_stats *out)
{
    if(!out||!s||!s->page_order||!exodus_page_store_validate(s))return false;
    struct exodus_page_pool_stats r={0};
    r.free_bytes=s->payload_capacity-s->payload_used;
    size_t cursor=0;
    for(size_t k=0;k<s->page_count;k++){
        size_t i=s->page_order[k],gap=s->page_starts[i]-cursor;
        if(gap)++r.holes;
        if(gap>r.largest_gap)r.largest_gap=gap;
        cursor=s->page_ends[i];
    }
    r.tail_free=s->payload_capacity-cursor;
    if(r.tail_free>r.largest_gap)r.largest_gap=r.tail_free;
    *out=r;return true;
}
bool exodus_page_store_pool_compact(struct exodus_page_store *s,size_t budget,size_t *moved)
{
    if(!moved||!s||!s->page_order||!exodus_page_store_validate(s))return false;
    size_t cursor=0,total=0;
    for(size_t k=0;k<s->page_count;k++){
        size_t i=s->page_order[k],a=s->page_starts[i],n=s->page_ends[i]-a;
        if(a!=cursor){
            if(n>budget-total)break;
            memmove(s->payload+cursor,s->payload+a,n);
            s->page_starts[i]=cursor;s->page_ends[i]=cursor+n;
            total+=n;
        }
        cursor+=n;
    }
    *moved=total;return true;
}

bool exodus_page_store_pool_put(struct exodus_page_store *s,size_t i,
                                const uint8_t *encoded,size_t n)
{
    if(!s||!s->page_starts||!encoded||!n||n>EXODUS_PAGE_BYTES||
       !exodus_page_store_validate(s)||i>s->page_count||
       (i==s->page_count&&i==s->page_capacity))return false;
    bool append=i==s->page_count;
    size_t old=append?0:s->page_ends[i]-s->page_starts[i];
    size_t other=s->payload_used-old;
    if(n>s->payload_capacity-other)return false;
    size_t limit=s->payload_capacity-n;
    size_t a=append?0:s->page_starts[i];
    if(a>limit||conflict(s,i,a,n)!=a){
        if(s->page_order){
            if(!ordered_gap(s,i,n,&a))return false;
        }else{
        a=0;
        for(;;){
            if(a>limit)return false;
            size_t next=conflict(s,i,a,n);
            if(next==a)break;
            a=next;
        }
        }
    }
    if(s->page_order&&!append)order_remove(s,i);
    memcpy(s->payload+a,encoded,n);
    s->page_starts[i]=a;s->page_ends[i]=a+n;s->payload_used=other+n;
    if(s->page_order)order_insert(s,s->page_count-(append?0:1),i);
    if(append)++s->page_count;
    return true;
}

bool exodus_page_store_pool_remove(struct exodus_page_store *s,size_t i)
{
    if(!s||!s->page_starts||!exodus_page_store_validate(s)||i>=s->page_count)return false;
    size_t bytes=s->page_ends[i]-s->page_starts[i];
    if(s->page_order){
        order_remove(s,i);
        for(size_t k=0;k+1<s->page_count;k++)if(s->page_order[k]>i)--s->page_order[k];
    }
    for(size_t j=i+1;j<s->page_count;j++){
        s->page_starts[j-1]=s->page_starts[j];
        s->page_ends[j-1]=s->page_ends[j];
    }
    --s->page_count;s->payload_used-=bytes;
    return true;
}

static bool append_page(struct exodus_page_store *store, const uint8_t *page, bool adaptive)
{
    if (!store || !page || !store->payload || !store->page_ends ||
        store->page_count >= store->page_capacity ||
        store->payload_used > store->payload_capacity)
        return false;

    uint8_t encoded[EXODUS_PAGE_BYTES];
    size_t size = adaptive ? exodus_page_encode_adaptive(page, encoded)
                           : exodus_page_encode(page, encoded);
    if (store->page_starts)
        return exodus_page_store_pool_put(store,store->page_count,encoded,size);
    if (size > store->payload_capacity - store->payload_used)
        return false;

    memcpy(store->payload + store->payload_used, encoded, size);
    store->payload_used += size;
    store->page_ends[store->page_count] = store->payload_used;
    ++store->page_count;
    return true;
}

bool exodus_page_store_append(struct exodus_page_store *store, const uint8_t *page)
{
    return append_page(store, page, false);
}

bool exodus_page_store_append_adaptive(struct exodus_page_store *store, const uint8_t *page)
{
    return append_page(store, page, true);
}

bool exodus_page_store_restore(const struct exodus_page_store *store, size_t index, uint8_t *page)
{
    if (!store || !page || !store->payload || !store->page_ends ||
        index >= store->page_count || store->page_count > store->page_capacity ||
        store->payload_used > store->payload_capacity)
        return false;

    size_t start,end;
    if(!exodus_page_store_extent(store,index,&start,&end))return false;

    return exodus_page_decode(store->payload + start, end - start, page);
}

static bool replace_page(struct exodus_page_store *store, size_t index,
                         const uint8_t *page, bool adaptive)
{
    if (!store || !page || !store->payload || !store->page_ends ||
        index >= store->page_count || store->page_count > store->page_capacity ||
        store->payload_used > store->payload_capacity)
        return false;

    if(store->page_starts){
        uint8_t encoded[EXODUS_PAGE_BYTES];
        size_t n=adaptive?exodus_page_encode_adaptive(page,encoded):exodus_page_encode(page,encoded);
        return exodus_page_store_pool_put(store,index,encoded,n);
    }

    /* Every following offset will move with the tail. Validate the entire
     * index before touching either buffer. */
    size_t previous = 0;
    for (size_t i = 0; i < store->page_count; ++i) {
        size_t end = store->page_ends[i];
        if (end <= previous || end > store->payload_used || end - previous > EXODUS_PAGE_BYTES)
            return false;
        previous = end;
    }
    if (previous != store->payload_used)
        return false;

    size_t start = index ? store->page_ends[index - 1] : 0;
    size_t end = store->page_ends[index];
    size_t old_size = end - start;
    uint8_t encoded[EXODUS_PAGE_BYTES];
    size_t size = adaptive ? exodus_page_encode_adaptive(page, encoded)
                           : exodus_page_encode(page, encoded);
    size_t other_bytes = store->payload_used - old_size;
    if (size > store->payload_capacity - other_bytes)
        return false;

    memmove(store->payload + start + size, store->payload + end, store->payload_used - end);
    memcpy(store->payload + start, encoded, size);
    for (size_t i = index; i < store->page_count; ++i) {
        if (size >= old_size)
            store->page_ends[i] += size - old_size;
        else
            store->page_ends[i] -= old_size - size;
    }
    store->payload_used = other_bytes + size;
    return true;
}

bool exodus_page_store_replace(struct exodus_page_store *store, size_t index, const uint8_t *page)
{
    return replace_page(store, index, page, false);
}

bool exodus_page_store_replace_adaptive(struct exodus_page_store *store, size_t index,
                                        const uint8_t *page)
{
    return replace_page(store, index, page, true);
}
