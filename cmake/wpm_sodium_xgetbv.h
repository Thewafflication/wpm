/* TinyCC accepts register names without '%' in clobbers. Supply the XGETBV
 * hook instead of libsodium's GCC-specific inline-assembly fallback. The
 * runtime detector calls this only after checking AVX, XSAVE and OSXSAVE. */
#ifndef WPM_SODIUM_XGETBV_H
#define WPM_SODIUM_XGETBV_H
static unsigned long long wpm_sodium_xgetbv(unsigned int index) {
    unsigned int low, high;
    __asm__ __volatile__(".byte 0x0f, 0x01, 0xd0"
        : "=a"(low), "=d"(high) : "c"(index));
    return ((unsigned long long)high << 32) | low;
}
#define _xgetbv wpm_sodium_xgetbv
#endif
