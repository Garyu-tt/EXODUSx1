/* Compile-only ISA reference; never execute against physical shared RAM.
 * The presence of these opcodes says nothing about the SoC interconnect.
 */
typedef __UINT32_TYPE__ u32;
typedef __UINT64_TYPE__ u64;
_Static_assert(sizeof(u32)==4 && sizeof(u64)==8, "wire widths");
_Static_assert(__atomic_always_lock_free(4,0), "32-bit local atomic");
_Static_assert(__atomic_always_lock_free(8,0), "64-bit local atomic");
u64 claim64(u64 *p, u64 desired) {
    u64 expected=0;
    __atomic_compare_exchange_n(p,&expected,desired,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);
    return expected;
}
void publish32(u32 *p,u32 value) {
    __atomic_store_n(p,value,__ATOMIC_RELEASE);
}
u32 observe32(const u32 *p) {
    return __atomic_load_n(p,__ATOMIC_ACQUIRE);
}
