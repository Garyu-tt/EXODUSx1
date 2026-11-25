#include "page_dedup_store.h"

#include <string.h>

static const uint8_t placeholder[8]={0};
static struct exodus_hot_entry *hot_find(struct exodus_page_dedup_store *s,size_t physical)
{
    return (struct exodus_hot_entry *)exodus_hot_find(s,physical);
}
static struct exodus_hot_entry *hot_free(struct exodus_page_dedup_store *s)
{
    if(!s->slab||!s->hot)return NULL;
    for(size_t i=0;i<s->hot_capacity;i++)if(!s->hot[i].page.active)return &s->hot[i];
    return NULL;
}
static bool restore_physical(const struct exodus_page_dedup_store *s,size_t i,uint8_t *out)
{
    if(i>=s->unique.page_count)return false;
    const struct exodus_hot_entry *h=exodus_hot_find(s,i);
    return h?exodus_bdi_slab_restore(s->slab,&h->page,out):exodus_page_store_restore(&s->unique,i,out);
}
/* Compare raw and BDI cold records without materializing a full page.
 * Other encodings and hot records retain the validated restore path.
 * Distinguish invalid backing from a valid unequal page. */
static bool compare_physical(const struct exodus_page_dedup_store *s,size_t i,
                             const uint8_t *page,bool *equal)
{
    size_t start,end;
    if(i>=s->unique.page_count)return false;
    if(!exodus_hot_find(s,i)){
        if(!exodus_page_store_extent(&s->unique,i,&start,&end))return false;
        if(end-start==EXODUS_PAGE_BYTES){
            *equal=memcmp(s->unique.payload+start,page,EXODUS_PAGE_BYTES)==0;
            return true;
        }
        size_t size=end-start;
        const uint8_t *src=s->unique.payload+start;
        if((size&1u)&&src[0]==EXODUS_PAGE_BDI_VERSION){
            /* The word reader validates the complete V3 layout before reading.
             * Equality alone conflates malformed backing and a mismatch. */
            uint8_t word[4];
            if(!exodus_page_bdi_read_word(src,size,0,word))return false;
            *equal=exodus_page_bdi_equal_page(src,size,page);
            return true;
        }
    }
    uint8_t restored[EXODUS_PAGE_BYTES];
    if(!restore_physical(s,i,restored))return false;
    *equal=memcmp(restored,page,sizeof restored)==0;
    return true;
}

static void hot_shift(struct exodus_page_dedup_store *s,size_t old)
{
    for(size_t i=0;i<s->hot_capacity;i++)
        if(s->hot[i].page.active&&s->hot[i].physical>old)--s->hot[i].physical;
}

static bool valid_store(const struct exodus_page_dedup_store *store)
{
    return store && store->fingerprints && store->page_refs &&
        (!store->heat||(store->heat->logical&&store->heat->scratch&&store->slab)) &&
        ((!store->slab&&!store->hot&&!store->hot_capacity)||
         (store->slab&&store->hot&&store->hot_capacity&&store->unique.page_order)) &&
        store->unique.payload && exodus_page_store_has_index(&store->unique) &&
        store->page_count <= store->page_capacity &&
        store->unique.page_count <= store->unique.page_capacity &&
        store->unique.page_count <= store->page_count &&
        ((store->unique.page_count == 0) == (store->page_count == 0)) &&
        store->unique.payload_used <= store->unique.payload_capacity &&
        ((store->unique.page_count == 0) == (store->unique.payload_used == 0)) &&
        (exodus_page_store_is_pool(&store->unique) || store->unique.page_count == 0 ||
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
        bool equal;
        if (!compare_physical(store,i,page,&equal))
            return false;
        if (equal) {
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
    if(store->heat)store->heat->logical[store->page_count]=0;
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
    return restore_physical(store,store->page_refs[index],page);
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
    if(exodus_page_store_is_pool(u)){
        struct exodus_hot_entry *h=hot_find(s,old);
        if(match==u->page_count){
            size_t target=owners==1?old:match;
            if(owners>1&&u->page_count>=s->page_count)return false;
            if(!exodus_page_store_pool_put(u,target,encoded,size))return false;
            if(owners==1&&h)exodus_bdi_slab_release(s->slab,&h->page);
            s->fingerprints[target]=hash;
            s->page_refs[index]=target;
        }else{
            if(owners==1){
                if(!exodus_page_store_pool_remove(u,old))return false;
                if(h)exodus_bdi_slab_release(s->slab,&h->page);
                hot_shift(s,old);
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

static bool replace_adaptive(struct exodus_page_dedup_store *s,
                                             size_t index,const uint8_t *page)
{
    size_t old,owners;
    if(!page||!replacement_context(s,index,&old,&owners))return false;
    bool equal;
    if(!compare_physical(s,old,page,&equal))return false;
    if(equal)return true;
    uint64_t hash=exodus_page_fingerprint(page);
    size_t match=s->unique.page_count;
    for(size_t i=0;i<s->unique.page_count;i++){
        if(i==old||s->fingerprints[i]!=hash)continue;
        if(!compare_physical(s,i,page,&equal))return false;
        if(equal){match=i;break;}
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
    if(hot_find(s,old)){*handled=false;return false;}
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
    struct exodus_bdi_patch patch;
    if(!exodus_page_bdi_prepare_patch(src,size,word,value,&patch))return false;
    size_t n=size-patch.old_len+patch.new_len;
    if(n>=EXODUS_PAGE_BYTES){*handled=false;return false;}
    uint64_t hash;
    if(!exodus_page_fingerprint_update(s->fingerprints[old],word,current,value,&hash))return false;
    size_t match=s->unique.page_count;
    for(size_t i=0;i<s->unique.page_count;i++){
        if(i==old||s->fingerprints[i]!=hash)continue;
        uint8_t candidate[EXODUS_PAGE_BYTES];
        if(!restore_physical(s,i,candidate))return false;
        /* Compare candidate with the prospective page without copying it:
         * undo the single edited word in the candidate scratch buffer. */
        if(memcmp(candidate+4*word,value,4)!=0)continue;
        memcpy(candidate+4*word,current,4);
        if(exodus_page_bdi_equal_page(src,size,candidate)){match=i;break;}
    }
    if(match!=s->unique.page_count)
        return commit_replacement(s,index,old,owners,match,hash,NULL,0);
    if(owners==1&&patch.old_len==patch.new_len){
        /* No size change, no aliases and no equal candidate. All checks
         * completed: update only the affected block and its fingerprint. */
        memcpy(s->unique.payload+start+patch.offset,patch.bytes,patch.new_len);
        s->fingerprints[old]=hash;
        return true;
    }
    uint8_t encoded[EXODUS_PAGE_BYTES];
    if(exodus_page_bdi_patch_word(src,size,word,value,encoded)!=n)return false;
    return commit_replacement(s,index,old,owners,match,hash,encoded,n);
}

/* Hot writes retain the global fingerprint index and exact equality check. */
static bool write_hot(struct exodus_page_dedup_store *s,size_t index,size_t word,
                      const uint8_t value[4],bool *handled)
{
    *handled=false;
    if(!valid_store(s)||index>=s->page_count)return false;
    struct exodus_hot_entry *h=hot_find(s,s->page_refs[index]);
    if(!h)return false;
    *handled=true;
    size_t old,owners;
    if(!replacement_context(s,index,&old,&owners))return false;
    uint8_t before[4];
    if(!exodus_bdi_slab_read(s->slab,&h->page,word,before))return false;
    if(!memcmp(before,value,4))return true;
    uint64_t hash;
    if(!exodus_page_fingerprint_update(s->fingerprints[old],word,before,value,&hash))return false;
    for(size_t i=0;i<s->unique.page_count;i++){
        if(i==old||s->fingerprints[i]!=hash)continue;
        const struct exodus_hot_entry *candidate=exodus_hot_find(s,i);
        bool equal;
        if(candidate){
            if(!exodus_bdi_slab_compare_patched(s->slab,&h->page,word,value,
                                              &candidate->page,&equal))return false;
        }else{
            uint8_t a[EXODUS_PAGE_BYTES];
            if(!restore_physical(s,old,a))return false;
            memcpy(a+4*word,value,4);
            if(!compare_physical(s,i,a,&equal))return false;
        }
        if(equal)return commit_replacement(s,index,old,owners,i,hash,NULL,0);
    }
    if(owners==1){
        if(!exodus_bdi_slab_write(s->slab,&h->page,word,value)){*handled=false;return false;}
        s->fingerprints[old]=hash;return true;
    }
    struct exodus_hot_entry *dst=hot_free(s);
    size_t target=s->unique.page_count;
    if(!dst||target>=s->page_count||!exodus_page_store_pool_can_put(&s->unique,target,sizeof placeholder)){
        *handled=false;return false;
    }
    struct exodus_bdi_slab_page clone={0};
    if(!exodus_bdi_slab_clone(s->slab,&clone,&h->page)){*handled=false;return false;}
    if(!exodus_bdi_slab_write(s->slab,&clone,word,value)){
        exodus_bdi_slab_release(s->slab,&clone);*handled=false;return false;
    }
    /* Same preflight, serialized and unchanged cold pool: put cannot fail. */
    if(!exodus_page_store_pool_put(&s->unique,target,placeholder,sizeof placeholder)){
        exodus_bdi_slab_release(s->slab,&clone);return false;
    }
    dst->physical=target;dst->page=clone;
    s->fingerprints[target]=hash;s->page_refs[index]=target;return true;
}

bool exodus_page_dedup_store_promote(struct exodus_page_dedup_store *s,size_t index)
{
    size_t old,owners;
    if(!replacement_context(s,index,&old,&owners)||!s->slab)return false;
    if(hot_find(s,old))return true;
    struct exodus_hot_entry *h=hot_free(s);
    uint8_t page[EXODUS_PAGE_BYTES];
    if(!h||!restore_physical(s,old,page)||!exodus_page_store_pool_can_put(&s->unique,old,sizeof placeholder))return false;
    struct exodus_bdi_slab_page tmp={0};
    if(!exodus_bdi_slab_import(s->slab,&tmp,page))return false;
    if(!exodus_page_store_pool_put(&s->unique,old,placeholder,sizeof placeholder)){
        exodus_bdi_slab_release(s->slab,&tmp);return false;
    }
    h->physical=old;h->page=tmp;return true;
}

bool exodus_page_dedup_store_demote(struct exodus_page_dedup_store *s,size_t index)
{
    size_t old,owners;
    if(!replacement_context(s,index,&old,&owners))return false;
    struct exodus_hot_entry *h=hot_find(s,old);
    if(!h)return true;
    uint8_t page[EXODUS_PAGE_BYTES];
    if(!restore_physical(s,old,page)||!exodus_page_store_replace_adaptive(&s->unique,old,page))return false;
    return exodus_bdi_slab_release(s->slab,&h->page);
}

static bool write_word(struct exodus_page_dedup_store *s,
                                       size_t index, size_t word,
                                       const uint8_t value[4])
{
    if (!value || word >= EXODUS_PAGE_BYTES / 4) return false;
    bool handled;
    bool result=write_hot(s,index,word,value,&handled);
    if(handled)return result;
    result=write_bdi(s,index,word,value,&handled);
    if(handled)return result;
    uint8_t page[EXODUS_PAGE_BYTES];
    if(!exodus_page_dedup_store_restore(s,index,page))return false;
    memcpy(page + word * 4, value, 4);
    return replace_adaptive(s, index, page);
}

/* Successful write requests, including no-ops, count once. */
static void record_heat(struct exodus_page_dedup_store *s,size_t index)
{
    if(s->heat&&s->heat->logical[index]<UINT8_MAX)++s->heat->logical[index];
}

bool exodus_page_dedup_store_replace_adaptive(struct exodus_page_dedup_store *s,
                                             size_t index,const uint8_t *page)
{
    if(!replace_adaptive(s,index,page))return false;
    record_heat(s,index);return true;
}

bool exodus_page_dedup_store_write_word(struct exodus_page_dedup_store *s,
                                       size_t index,size_t word,const uint8_t value[4])
{
    if(!write_word(s,index,word,value))return false;
    record_heat(s,index);return true;
}

bool exodus_page_dedup_store_heat_tick(struct exodus_page_dedup_store *s,
    uint8_t promote,uint8_t demote,size_t min_cold_bytes,size_t budget,
    struct exodus_heat_result *out)
{
    if(!out||!valid_store(s)||!s->heat||!s->slab||promote<=demote||
       min_cold_bytes>EXODUS_PAGE_BYTES||!exodus_page_store_validate(&s->unique))return false;
    for(size_t i=0;i<s->page_count;i++)if(s->page_refs[i]>=s->unique.page_count)return false;
    struct exodus_page_heat *h=s->heat;
    struct exodus_heat_result r={0};
    for(size_t i=0;i<s->unique.page_count;i++)h->scratch[i]=0;
    for(size_t i=0;i<s->page_count;i++){
        size_t p=s->page_refs[i];
        if(h->logical[i]>h->scratch[p])h->scratch[p]=h->logical[i];
    }
    size_t count=s->page_count;
    size_t begin=count?h->cursor%count:0,next=begin;
    /* Demote first to make space. Mark attempted physical pages so aliases
     * cannot consume the budget twice, even if a migration fails. */
    for(unsigned phase=0;phase<2;phase++){
        size_t i=begin;
        for(size_t k=0;k<count&&r.attempted<budget;k++){
            size_t p=s->page_refs[i];uint16_t score=h->scratch[p];
            bool hot=exodus_hot_find(s,p)!=NULL;
            if(!(score&256u)&&((phase==0&&hot&&score<=demote)||
                              (phase==1&&!hot&&score>=promote))){
                size_t a,b;
                if(phase==0||(exodus_page_store_extent(&s->unique,p,&a,&b)&&b-a>=min_cold_bytes)){
                    h->scratch[p]|=256u;++r.attempted;
                    bool ok=phase==0?exodus_page_dedup_store_demote(s,i):exodus_page_dedup_store_promote(s,i);
                    if(!ok)++r.failed;else if(phase==0)++r.demoted;else ++r.promoted;
                    next=i+1==count?0:i+1;
                }
            }
            i=i+1==count?0:i+1;
        }
    }
    /* One epoch of decay, even when the migration budget is zero. */
    for(size_t i=0;i<count;i++)h->logical[i]/=2;
    h->cursor=r.attempted?next:(count&&begin+1<count?begin+1:0);
    *out=r;return true;
}

bool exodus_page_dedup_store_compact(struct exodus_page_dedup_store *s,
                                     size_t budget,size_t *moved)
{
    if(!valid_store(s))return false;
    for(size_t i=0;i<s->page_count;i++)
        if(s->page_refs[i]>=s->unique.page_count)return false;
    return exodus_page_store_pool_compact(&s->unique,budget,moved);
}

bool exodus_page_dedup_store_discard_free(struct exodus_page_dedup_store *s,
                                         size_t granule,size_t budget,
                                         exodus_page_discard_fn discard,void *context,
                                         size_t *discarded)
{
    if(!valid_store(s))return false;
    for(size_t i=0;i<s->page_count;i++)
        if(s->page_refs[i]>=s->unique.page_count)return false;
    return exodus_page_store_pool_discard_free(&s->unique,granule,budget,discard,context,discarded);
}
