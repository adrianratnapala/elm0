/*----------------------------------------------------------------------------
  0unit.h: unit testing without a real framework.

  These are minute definitoins to help you write unit tests.  See 0example.c.

  Copyright (C) 2012, Adrian Ratnapala, under the ISC license. See file LICENSE.
*/


#ifndef _0UNIT_H
#define _0UNIT_H

#include <stdio.h>
#include <stdarg.h>

#ifndef CHECK_FMT
# ifdef __GNUC__
#  define CHECK_FMT(N) __attribute__((format(printf,(N),(N)+1)))
# else
#  define CHECK_FMT(N)
# endif
#endif

#define CHK(T) do {\
                        if(!chk(!!(T), __FILE__, __LINE__, __func__, "%s", #T))\
                                goto fail; \
               } while(0)

#define FAIL(M, ...) do {\
                        if(!chk(0, __FILE__, __LINE__, __func__, \
                                  M, __VA_ARGS__)) goto fail;    \
               } while(0)

#define WRN(M) wrn( 0, __FILE__, __LINE__, __func__, M)


#define PASS()                   \
        return pass( __func__ ); \
        goto fail;               \
        fail:                    \
        return 0

#define PASS_ONLY()              \
        return pass( __func__ ); \
        goto fail;               \

#define PASS_QUIETLY()           \
        return 1;                \
        goto fail;               \
        fail:                    \
        return 0

inline static int fail(const char *prefix,
                       const char *file, int line, const char *test,
                       const char *fmt, va_list va)
{
        printf("%s %s:%d:%s <", prefix, file, line, test);
        vprintf(fmt, va);
        printf(">\n");
        fflush(stdout);
        va_end(va);
        return 0;
}

CHECK_FMT(5)
inline static int chk(int pass, const char *file, int line, const char *test,
                                const char *fmt, ...)
{
        if( pass )
                return 1;
        va_list va;
        va_start(va, fmt);
        return fail("\033[31m\033[1mFAILED:\033[0m", file, line, test, fmt, va);
}

CHECK_FMT(5)
inline static int wrn(int pass, const char *file, int line, const char *test,
                                const char *fmt, ...)
{
        if( pass )
                return 1;
        va_list va;
        va_start(va, fmt);
        return fail("\033[34m\033[1mWARNING:\033[0m", file, line, test, fmt, va);
}


inline static int pass(const char *test) {
        printf("\033[32m\033[1mpassed:\033[0m %s\n", test);
        return 1;
}

#endif /* _0UNIT_H */
