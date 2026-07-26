#include <stdio.h>

/* TinyCC's current headers target newer MSVCRT versions and emit an import
 * for __iob_func. Windows XP's MSVCRT exports the legacy _iob array instead. */
#undef __iob_func
__declspec(dllimport) extern FILE _iob[];

FILE *__cdecl __iob_func(void)
{
    return _iob;
}
