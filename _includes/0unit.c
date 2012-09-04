#include <0unit.h>
#include <string.h>

static int test_something_good()
{
        int x = 3;

        CHK(42 == 6 * 7);
        CHK(strlen("answer") < strlen("question"));
        CHK(4 == ++x);
        CHK(5 == ++x);

        PASS();
}

int main()
{
        test_something_good();
}

