#include "page_dedup_store.h"

#include <string.h>

static bool valid_store(const struct exodus_page_dedup_store *store)
{
    return store && store->fingerprints && store->page_refs &&
        store->unique.payload && store->unique.page_ends &&
        store->page_count <= store->page_capacity &&
        store->unique.page_count <= store->unique.page_capacity &&
        store->unique.page_count <= store->page_count &&
        ((store->unique.page_count == 0) == (store->page_count == 0)) &&
        store->unique.payload_used <= store->unique.payload_capacity &&
        ((store->unique.page_count == 0) == (store->unique.payload_used == 0)) &&
        (store->unique.page_starts || store->unique.page_count == 0 ||
         store->unique.page_ends[store->unique.page_count - 1] == store->unique.payload_used);
}

static bool append_page(struct exodus_page_dedup_store *store,
                        const uint8_t *page, bool adaptive)
{
    if (!page || !valid_store(store) || store->page_count == store->page_capacity)
        return false;

    /* Appending must not extend an inconsistent physical index, even
     * when no fingerprint matches or an early match would skip its tail.
     * Restore keeps its indexed, single-record validation path. */
    if(!exodus_page_store_validate(&store->unique))return false;

    uint64_t hash = exodus_page_fingerprint(page);
    size_t physical = store->unique.page_count;
    for (size_t i = 0; i < store->unique.page_count; ++i) {
        if (store->fingerprints[i] != hash)
            continue;
        uint8_t restored[EXODUS_PAGE_BYTES];
        if (!exodus_page_store_restore(&store->unique, i, restored))
            return false;
        if (memcmp(restored, page, sizeof(restored)) == 0) {
            physical = i;
            break;
        }
    }

    if (physical == store->unique.page_count) {
        if (!(adaptive ? exodus_page_store_append_adaptive(&store->unique, page)
                       : exodus_page_store_append(&store->unique, page)))
            return false;
        store->fingerprints[physical] = hash;
    }
    store->page_refs[store->page_count] = physical;
    ++store->page_count;
    return true;
}

bool exodus_page_dedup_store_append(struct exodus_page_dedup_store *store,
                                   const uint8_t *page)
{
    return append_page(store, page, false);
}

bool exodus_page_dedup_store_append_adaptive(struct exodus_page_dedup_store *store,
                                            const uint8_t *page)
{
    return append_page(store, page, true);
}

bool exodus_page_dedup_store_restore(const struct exodus_page_dedup_store *store,
                                    size_t index, uint8_t *page)
{
    if (!page || !valid_store(store) || index >= store->page_count)
        return false;
    return exodus_page_store_restore(&store->unique, store->page_refs[index], page);
}

/* Validate all offsets and logical references before any replacement. */
static bool replacement_context(const struct exodus_page_dedup_store *s,size_t index,
                                size_t *old,size_t *owners)
{
    if(!valid_store(s)||index>=s->page_count)return false;
    if(!exodus_page_store_validate(&s->unique))return false;
    *old=s->page_refs[index];*owners=0;
    if(*old>=s->unique.page_count)return false;
    for(size_t i=0;i<s->page_count;i++){
        if(s->page_refs[i]>=s->unique.page_count)return false;
        if(s->page_refs[i]==*old)++*owners;
    }
    return true;
}

/* Commit a validated encoded replacement. No mutations before budget checks.
 * Logical page indices are stable; physical records may shift. */
static bool commit_replacement(struct exodus_page_dedup_store *s,size_t index,
                               size_t old,size_t owners,size_t match,uint64_t hash,
                               const uint8_t *encoded,size_t size)
{
    struct exodus_page_store *u=&s->unique;
    if(u->page_starts){
        if(match==u->page_count){
            size_t target=owners==1?old:match;
            if(owners>1&&u->page_count>=s->page_count)return false;
            if(!exodus_page_store_pool_put(u,target,encoded,size))return false;
            s->fingerprints[target]=hash;
            s->page_refs[index]=target;
        }else{
            if(owners==1){
                if(!exodus_page_store_pool_remove(u,old))return false;
                for(size_t i=old;i<u->page_count;i++)s->fingerprints[i]=s->fingerprints[i+1];
                for(size_t i=0;i<s->page_count;i++)if(s->page_refs[i]>old)--s->page_refs[i];
                if(match>old)--match;
            }
            s->page_refs[index]=match;
        }
        return true;
    }
    size_t start=old?u->page_ends[old-1]:0,end=u->page_ends[old],bytes=end-start;
    if(match==u->page_count){
        if(!encoded||!size||size>EXODUS_PAGE_BYTES)return false;
        if(owners==1){
            size_t other=u->payload_used-bytes;
            if(size>u->payload_capacity-other)return false;
            if(size!=bytes)memmove(u->payload+start+size,u->payload+end,u->payload_used-end);
            memcpy(u->payload+start,encoded,size);
            for(size_t i=old;i<u->page_count;i++){
                if(size>=bytes)u->page_ends[i]+=size-bytes;
                else u->page_ends[i]-=bytes-size;
            }
            u->payload_used=other+size;
            s->fingerprints[old]=hash;
        }else{
            if(u->page_count>=u->page_capacity||u->page_count>=s->page_count||
               size>u->payload_capacity-u->payload_used)return false;
            memcpy(u->payload+u->payload_used,encoded,size);
            u->payload_used+=size;
            u->page_ends[match]=u->payload_used;
            s->fingerprints[match]=hash;
            ++u->page_count;
            s->page_refs[index]=match;
        }
        return true;
    }
    s->page_refs[index]=match;
    if(owners==1){
        memmove(u->payload+start,u->payload+end,u->payload_used-end);
        for(size_t i=old+1;i<u->page_count;i++){
            u->page_ends[i-1]=u->page_ends[i]-bytes;
            s->fingerprints[i-1]=s->fingerprints[i];
        }
        for(size_t i=0;i<s->page_count;i++)if(s->page_refs[i]>old)--s->page_refs[i];
        --u->page_count;
        u->payload_used-=bytes;
    }
    return true;
}

bool exodus_page_dedup_store_replace_adaptive(struct exodus_page_dedup_store *s,
                                             size_t index,const uint8_t *page)
{
    size_t old,owners;
    if(!page||!replacement_context(s,index,&old,&owners))return false;
    uint8_t restored[EXODUS_PAGE_BYTES];
    if(!exodus_page_store_restore(&s->unique,old,restored))return false;
    if(memcmp(restored,page,sizeof(restored))==0)return true;
    uint64_t hash=exodus_page_fingerprint(page);
    size_t match=s->unique.page_count;
    for(size_t i=0;i<s->unique.page_count;i++){
        if(i==old||s->fingerprints[i]!=hash)continue;
        if(!exodus_page_store_restore(&s->unique,i,restored))return false;
        if(memcmp(restored,page,sizeof(restored))==0){match=i;break;}
    }
    if(match!=s->unique.page_count)
        return commit_replacement(s,index,old,owners,match,hash,NULL,0);
    uint8_t encoded[EXODUS_PAGE_BYTES];
    size_t size=exodus_page_encode_adaptive(page,encoded);
    return commit_replacement(s,index,old,owners,match,hash,encoded,size);
}

/* Fast path: only patch one BDI block. The logical hash is updated in O(1), and
 * candidate pages are decoded only on hash matches, never on a miss. */
static bool write_bdi(struct exodus_page_dedup_store *s,size_t index,size_t word,
                      const uint8_t value[4],bool *handled)
{
    size_t old,owners;
    *handled=true;
    if(!replacement_context(s,index,&old,&owners))return false;
    size_t start,end;
    if(!exodus_page_store_extent(&s->unique,old,&start,&end))return false;
    size_t size=end-start;
    const uint8_t *src=s->unique.payload+start;
    if(size==EXODUS_PAGE_BYTES||!(size&1u)||src[0]!=EXODUS_PAGE_BDI_VERSION){
        *handled=false;return false;
    }
    uint8_t current[4];
    if(!exodus_page_bdi_read_word(src,size,word,current))return false;
    if(memcmp(current,value,4)==0)return true;
    uint8_t encoded[EXODUS_PAGE_BYTES];
    size_t n=exodus_page_bdi_patch_word(src,size,word,value,encoded);
    if(!n){*handled=false;return false;}
    uint64_t hash;
    if(!exodus_page_fingerprint_update(s->fingerprints[old],word,current,value,&hash))return false;
    size_t match=s->unique.page_count;
    for(size_t i=0;i<s->unique.page_count;i++){
        if(i==old||s->fingerprints[i]!=hash)continue;
        uint8_t candidate[EXODUS_PAGE_BYTES];
        if(!exodus_page_store_restore(&s->unique,i,candidate))return false;
        if(exodus_page_bdi_equal_page(encoded,n,candidate)){match=i;break;}
    }
    return commit_replacement(s,index,old,owners,match,hash,encoded,n);
}

bool exodus_page_dedup_store_write_word(struct exodus_page_dedup_store *s,
                                       size_t index, size_t word,
                                       const uint8_t value[4])
{
    if (!value || word >= EXODUS_PAGE_BYTES / 4) return false;
    bool handled;
    bool result=write_bdi(s,index,word,value,&handled);
    if(handled)return result;
    uint8_t page[EXODUS_PAGE_BYTES];
    if(!exodus_page_dedup_store_restore(s,index,page))return false;
    memcpy(page + word * 4, value, 4);
    return exodus_page_dedup_store_replace_adaptive(s, index, page);
}
