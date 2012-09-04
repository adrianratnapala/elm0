#include <elm.h>

int forty_two=-3;

int main()
{
        LOG_F(std_log, "The answer is %d", forty_two);
        if(!forty_two)
                LOG_F(dbg_log, "Tell me where you are.");
        LOG_F(err_log, "But what is the question!");

        return 0;
}
