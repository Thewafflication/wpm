/* Runtime dispatch for the SSE2 kernels supported by TinyCC. */
#include "zbuild.h"
#include "deflate.h"
#include <wcrt/cpu.h>

void wpm_slide_hash_scalar(deflate_state *s);
void wpm_inflate_scalar(PREFIX3(stream) *strm, uint32_t start);
uint8_t *wpm_chunkmemset_scalar(uint8_t *out, uint8_t *from, unsigned len, unsigned left);
void slide_hash_sse2(deflate_state *s);
void inflate_fast_sse2(PREFIX3(stream) *strm, uint32_t start);
uint8_t *chunkmemset_safe_sse2(uint8_t *out, uint8_t *from, unsigned len, unsigned left);

void slide_hash_c(deflate_state *s)
{
    /* The upstream intrinsic kernel requires aligned tables. */
    if (wcrt_cpu_has_features(WCRT_CPU_SSE2) &&
        (((uintptr_t)s->head | (uintptr_t)s->prev) & 15) == 0)
        slide_hash_sse2(s);
    else
        wpm_slide_hash_scalar(s);
}

void inflate_fast_c(PREFIX3(stream) *strm, uint32_t start)
{
    if (wcrt_cpu_has_features(WCRT_CPU_SSE2))
        inflate_fast_sse2(strm, start);
    else
        wpm_inflate_scalar(strm, start);
}

uint8_t *chunkmemset_safe_c(uint8_t *out, uint8_t *from, unsigned len, unsigned left)
{
    if (wcrt_cpu_has_features(WCRT_CPU_SSE2))
        return chunkmemset_safe_sse2(out, from, len, left);
    return wpm_chunkmemset_scalar(out, from, len, left);
}
