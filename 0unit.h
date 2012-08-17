/*----------------------------------------------------------------------------
  0unit.h: unit testing without a real framework.

  These are minute definitoins to help you write unit tests.  See 0example.c.

  Copyright (C) 2012, Adrian Ratnapala, under the ISC license. See file LICENSE.
*/


#ifndef _0UNIT_H
#define _0UNIT_H

// FIX: CHK should goto fail;

#include <stdio.h>
#define CHK(T) do {\
                        if( !chk( !!(T), __FILE__, __LINE__, __func__, #T) ) \
                                return 0; \
               } while(0)

#define WRN(T) wrn( !!(T), __FILE__, __LINE__, __func__, #T)

#define PASS() return pass( __func__ )

inline static int chk(int pass, const char *file, int line,
                                const char *test, const char *text)
{
        if( pass )
                return 1;
        printf("\033[31m\033[1mFAILED:\033[0m %s:%d:%s <%s>\n",
                file, line, test, text);
        return 0;
}

inline static int wrn(int pass, const char *file, int line,
                                const char *test, const char *text)
{
        if( pass )
                return 1;
        printf("\033[34m\033[1mWARNING:\033[0m %s:%d:%s <%s>\n",
                file, line,test, text);
        return 0;
}

inline static int pass(const char *test) {
        printf("\033[32m\033[1mpassed:\033[0m %s\n", test);
        return 1;
}

#endif /* _0UNIT_H */
