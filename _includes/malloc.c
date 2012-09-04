#include <elm.h>

int main
{
        // allocate memory AND handle errors
        char *garbage = MALLOC(20);
        int  *zeroes = ZALLOC(20);

        // use it without fear
        garbage[0] = '\0';

        return 0;
}
